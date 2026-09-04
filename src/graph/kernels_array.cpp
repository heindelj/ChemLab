// Native array kernels: the vectorised building blocks workflows are composed
// from. Each is a pure function of its inputs and parameters (see executor.h):
// no AppState, no ImGui, so they are usable from the node graph, the
// workflow executor, a command or a benchmark alike. Arrays are the graph's
// FloatVec (vector<double>) and IntVec (vector<int64_t>); a mask is an IntVec
// of 0/1.

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>

#include <fmt/format.h>

#include "graph/executor.h"

namespace graph {

namespace {

// ---- helpers ----

const Value* In(const KernelArgs& a, size_t k) { return k < a.nin ? a.in[k] : nullptr; }

enum class BinOp { Add, Sub, Mul, Div, Min, Max, Pow, Unknown };

BinOp ParseBinOp(const std::string& s) {
    if (s == "add" || s == "+") return BinOp::Add;
    if (s == "sub" || s == "-") return BinOp::Sub;
    if (s == "mul" || s == "*") return BinOp::Mul;
    if (s == "div" || s == "/") return BinOp::Div;
    if (s == "min") return BinOp::Min;
    if (s == "max") return BinOp::Max;
    if (s == "pow") return BinOp::Pow;
    return BinOp::Unknown;
}

inline double Apply(BinOp op, double x, double y) {
    switch (op) {
        case BinOp::Add: return x + y;
        case BinOp::Sub: return x - y;
        case BinOp::Mul: return x * y;
        case BinOp::Div: return x / y;
        case BinOp::Min: return std::min(x, y);
        case BinOp::Max: return std::max(x, y);
        case BinOp::Pow: return std::pow(x, y);
        default: return std::numeric_limits<double>::quiet_NaN();
    }
}

enum class CmpOp { Lt, Le, Gt, Ge, Eq, Ne, Unknown };

CmpOp ParseCmpOp(const std::string& s) {
    if (s == "lt" || s == "<") return CmpOp::Lt;
    if (s == "le" || s == "<=") return CmpOp::Le;
    if (s == "gt" || s == ">") return CmpOp::Gt;
    if (s == "ge" || s == ">=") return CmpOp::Ge;
    if (s == "eq" || s == "==") return CmpOp::Eq;
    if (s == "ne" || s == "!=") return CmpOp::Ne;
    return CmpOp::Unknown;
}

inline bool Apply(CmpOp op, double x, double y) {
    switch (op) {
        case CmpOp::Lt: return x < y;
        case CmpOp::Le: return x <= y;
        case CmpOp::Gt: return x > y;
        case CmpOp::Ge: return x >= y;
        case CmpOp::Eq: return x == y;
        case CmpOp::Ne: return x != y;
        default: return false;
    }
}

// The "b" operand of the vector kernels: an array (elementwise) or a scalar
// (broadcast). Returns false + err when it is neither.
struct Operand {
    const std::vector<double>* vec = nullptr;
    double scalar = 0.0;
    double At(size_t k) const { return vec ? (*vec)[k] : scalar; }
};

bool ReadOperand(const Value* v, const KernelArgs& a, const char* paramKey, Operand& out, std::string& err) {
    if (!v) {   // unconnected: the node's own constant
        out.scalar = a.FloatParam(paramKey, 0.0);
        return true;
    }
    if (const std::vector<double>* fv = v->AsFloatVec()) { out.vec = fv; return true; }
    if (const std::vector<int64_t>* iv = v->AsIntVec()) {
        // Promote once; cheap relative to the work that follows.
        static thread_local std::vector<double> promoted;
        promoted.assign(iv->begin(), iv->end());
        out.vec = &promoted;
        return true;
    }
    if (v->AsFloat(out.scalar)) return true;
    err = "operand 'b' must be a number or an array";
    return false;
}

// ---- kernels ----

// values[indices] for a FloatVec or IntVec.
std::string Gather(KernelArgs& a) {
    const Value* values = In(a, 0);
    const Value* indices = In(a, 1);
    if (!values) return "input 'values' not connected";
    if (!indices) return "input 'indices' not connected";
    const std::vector<int64_t>* idx = indices->AsIntVec();
    if (!idx) return "'indices' must be an int array";
    if (const std::vector<double>* fv = values->AsFloatVec()) {
        std::vector<double> out(idx->size());
        for (size_t k = 0; k < idx->size(); ++k) {
            const int64_t i = (*idx)[k];
            if (i < 0 || (size_t)i >= fv->size()) return fmt::format("index {} out of range 0..{}", i, (int64_t)fv->size() - 1);
            out[k] = (*fv)[(size_t)i];
        }
        a.out[0].v = std::move(out);
        return "";
    }
    if (const std::vector<int64_t>* iv = values->AsIntVec()) {
        std::vector<int64_t> out(idx->size());
        for (size_t k = 0; k < idx->size(); ++k) {
            const int64_t i = (*idx)[k];
            if (i < 0 || (size_t)i >= iv->size()) return fmt::format("index {} out of range 0..{}", i, (int64_t)iv->size() - 1);
            out[k] = (*iv)[(size_t)i];
        }
        a.out[0].v = std::move(out);
        return "";
    }
    return "'values' must be a float or int array";
}

// a (op) b elementwise; b an array or a scalar (pin, else the "b" param).
std::string VectorMath(KernelArgs& a) {
    const Value* va = In(a, 0);
    if (!va) return "input 'a' not connected";
    const std::vector<double>* x = va->AsFloatVec();
    std::vector<double> promotedA;
    if (!x) {
        const std::vector<int64_t>* ix = va->AsIntVec();
        if (!ix) return "'a' must be an array";
        promotedA.assign(ix->begin(), ix->end());
        x = &promotedA;
    }
    Operand b;
    std::string err;
    if (!ReadOperand(In(a, 1), a, "b", b, err)) return err;
    if (b.vec && b.vec->size() != x->size())
        return fmt::format("array lengths differ ({} vs {})", x->size(), b.vec->size());
    const BinOp op = ParseBinOp(a.TextParam("op", "add"));
    if (op == BinOp::Unknown) return "unknown op (add, sub, mul, div, min, max, pow)";
    std::vector<double> out(x->size());
    for (size_t k = 0; k < out.size(); ++k) out[k] = Apply(op, (*x)[k], b.At(k));
    a.out[0].v = std::move(out);
    return "";
}

// a (cmp) b elementwise -> mask of 0/1.
std::string Compare(KernelArgs& a) {
    const Value* va = In(a, 0);
    if (!va) return "input 'a' not connected";
    const std::vector<double>* x = va->AsFloatVec();
    std::vector<double> promotedA;
    if (!x) {
        const std::vector<int64_t>* ix = va->AsIntVec();
        if (!ix) return "'a' must be an array";
        promotedA.assign(ix->begin(), ix->end());
        x = &promotedA;
    }
    Operand b;
    std::string err;
    if (!ReadOperand(In(a, 1), a, "b", b, err)) return err;
    if (b.vec && b.vec->size() != x->size())
        return fmt::format("array lengths differ ({} vs {})", x->size(), b.vec->size());
    const CmpOp op = ParseCmpOp(a.TextParam("op", "lt"));
    if (op == CmpOp::Unknown) return "unknown op (lt, le, gt, ge, eq, ne)";
    std::vector<int64_t> mask(x->size());
    int64_t count = 0;
    for (size_t k = 0; k < mask.size(); ++k) {
        mask[k] = Apply(op, (*x)[k], b.At(k)) ? 1 : 0;
        count += mask[k];
    }
    a.out[0].v = std::move(mask);
    if (a.nout > 1) a.out[1] = Value::I(count);
    return "";
}

// Keep the entries of `values` where `mask` is non-zero.
std::string Filter(KernelArgs& a) {
    const Value* values = In(a, 0);
    const Value* mv = In(a, 1);
    if (!values) return "input 'values' not connected";
    if (!mv) return "input 'mask' not connected";
    const std::vector<int64_t>* mask = mv->AsIntVec();
    if (!mask) return "'mask' must be an int array";
    if (const std::vector<double>* fv = values->AsFloatVec()) {
        if (fv->size() != mask->size()) return fmt::format("mask length {} != values length {}", mask->size(), fv->size());
        std::vector<double> out;
        out.reserve(fv->size());
        for (size_t k = 0; k < fv->size(); ++k)
            if ((*mask)[k]) out.push_back((*fv)[k]);
        a.out[0].v = std::move(out);
        return "";
    }
    if (const std::vector<int64_t>* iv = values->AsIntVec()) {
        if (iv->size() != mask->size()) return fmt::format("mask length {} != values length {}", mask->size(), iv->size());
        std::vector<int64_t> out;
        out.reserve(iv->size());
        for (size_t k = 0; k < iv->size(); ++k)
            if ((*mask)[k]) out.push_back((*iv)[k]);
        a.out[0].v = std::move(out);
        return "";
    }
    return "'values' must be a float or int array";
}

// One number from an array: max, min, sum, mean or count.
std::string Reduce(KernelArgs& a) {
    const Value* va = In(a, 0);
    if (!va) return "input 'values' not connected";
    std::vector<double> promoted;
    const std::vector<double>* x = va->AsFloatVec();
    if (!x) {
        const std::vector<int64_t>* ix = va->AsIntVec();
        if (!ix) return "'values' must be an array";
        promoted.assign(ix->begin(), ix->end());
        x = &promoted;
    }
    const std::string op = a.TextParam("op", "max");
    if (op == "count") { a.out[0] = Value::I((int64_t)x->size()); return ""; }
    if (x->empty()) return "empty array";
    double r = 0.0;
    if (op == "max") r = *std::max_element(x->begin(), x->end());
    else if (op == "min") r = *std::min_element(x->begin(), x->end());
    else if (op == "sum") r = std::accumulate(x->begin(), x->end(), 0.0);
    else if (op == "mean") r = std::accumulate(x->begin(), x->end(), 0.0) / (double)x->size();
    else return "unknown op (max, min, sum, mean, count)";
    a.out[0] = Value::F(r);
    return "";
}

// Scalar a (op) b; b from the pin, else the "b" param.
std::string ScalarMath(KernelArgs& a) {
    const Value* va = In(a, 0);
    if (!va) return "input 'a' not connected";
    double x = 0.0, y = 0.0;
    if (!va->AsFloat(x)) return "'a' must be a number";
    if (const Value* vb = In(a, 1)) {
        if (!vb->AsFloat(y)) return "'b' must be a number";
    } else {
        y = a.FloatParam("b", 0.0);
    }
    const BinOp op = ParseBinOp(a.TextParam("op", "add"));
    if (op == BinOp::Unknown) return "unknown op (add, sub, mul, div, min, max, pow)";
    a.out[0] = Value::F(Apply(op, x, y));
    return "";
}

// Gate: passes `value` through when `pass` is non-zero, otherwise closes its
// output so everything downstream is skipped. "invert" flips the test.
std::string Gate(KernelArgs& a) {
    const Value* cond = In(a, 0);
    if (!cond) return "input 'pass' not connected";
    int64_t c = 0;
    double f = 0.0;
    if (cond->AsInt(c)) { /* ok */ }
    else if (cond->AsFloat(f)) c = f != 0.0;
    else return "'pass' must be a number (0 = closed)";
    if (a.IntParam("invert", 0)) c = !c;
    if (!c) { a.skip = true; return ""; }
    if (const Value* v = In(a, 1)) a.out[0] = *v;
    return "";
}

}  // namespace

void RegisterArrayKernels(KernelTable& t) {
    t.Register("array.gather", &Gather, "values[indices]");
    t.Register("array.math", &VectorMath, "elementwise a (op) b, b an array or a scalar");
    t.Register("array.compare", &Compare, "elementwise a (cmp) b -> 0/1 mask");
    t.Register("array.filter", &Filter, "entries of values where mask != 0");
    t.Register("array.reduce", &Reduce, "max / min / sum / mean / count of an array");
    t.Register("scalar.math", &ScalarMath, "a (op) b on numbers");
    t.Register("flow.gate", &Gate, "pass value through, or skip everything downstream");
}

}  // namespace graph
