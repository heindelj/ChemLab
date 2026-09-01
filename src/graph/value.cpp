#include "graph/value.h"

#include <fmt/format.h>
#include <nlohmann/json.hpp>

namespace graph {

using nlohmann::json;

ValueType Value::Type() const {
    switch (v.index()) {
        case 1: return ValueType::Float;
        case 2: return ValueType::Int;
        case 3: return ValueType::Text;
        case 4: return ValueType::FloatVec;
        case 5: return ValueType::Positions;
        case 6: return ValueType::Labels;
        case 7: return ValueType::IntVec;
        case 8: return ValueType::Chem;
        default: return ValueType::Any;   // monostate
    }
}

bool Value::AsFloat(double& out) const {
    if (auto* d = std::get_if<double>(&v)) { out = *d; return true; }
    if (auto* i = std::get_if<int64_t>(&v)) { out = (double)*i; return true; }
    return false;
}

bool Value::AsInt(int64_t& out) const {
    if (auto* i = std::get_if<int64_t>(&v)) { out = *i; return true; }
    if (auto* d = std::get_if<double>(&v)) { out = (int64_t)*d; return true; }
    return false;
}

const std::string* Value::AsText() const { return std::get_if<std::string>(&v); }
const std::vector<double>* Value::AsFloatVec() const { return std::get_if<std::vector<double>>(&v); }
const Positions* Value::AsPositions() const { return std::get_if<Positions>(&v); }
const Labels* Value::AsLabels() const { return std::get_if<Labels>(&v); }
const std::vector<int64_t>* Value::AsIntVec() const { return std::get_if<std::vector<int64_t>>(&v); }
const ChemicalData* Value::AsChem() const { return std::get_if<ChemicalData>(&v); }

std::string Value::Preview(size_t maxItems) const {
    switch (Type()) {
        case ValueType::Float: return fmt::format("{:.6g}", std::get<double>(v));
        case ValueType::Int: return fmt::format("{}", std::get<int64_t>(v));
        case ValueType::Text: {
            const std::string& s = std::get<std::string>(v);
            return s.size() <= 40 ? s : s.substr(0, 37) + "...";
        }
        case ValueType::FloatVec: {
            const auto& a = std::get<std::vector<double>>(v);
            std::string s = "[";
            for (size_t i = 0; i < a.size() && i < maxItems; ++i)
                s += fmt::format("{}{:.4g}", i ? ", " : "", a[i]);
            if (a.size() > maxItems) s += fmt::format(", ... ({} values)", a.size());
            return s + "]";
        }
        case ValueType::Positions: return fmt::format("positions[{} atoms]", std::get<Positions>(v).Count());
        case ValueType::Labels: return fmt::format("labels[{}]", std::get<Labels>(v).size());
        case ValueType::IntVec: {
            const auto& a = std::get<std::vector<int64_t>>(v);
            std::string s = "[";
            for (size_t i = 0; i < a.size() && i < maxItems; ++i) s += fmt::format("{}{}", i ? ", " : "", a[i]);
            if (a.size() > maxItems) s += fmt::format(", ... ({} values)", a.size());
            return s + "]";
        }
        case ValueType::Chem: {
            const auto& c = std::get<ChemicalData>(v);
            return fmt::format("chemdata[{} atoms, {} topolog{}]", c.natoms, c.topologies.size(),
                               c.topologies.size() == 1 ? "y" : "ies");
        }
        default: return "-";
    }
}

const char* TypeName(ValueType t) {
    switch (t) {
        case ValueType::Float: return "float";
        case ValueType::Int: return "int";
        case ValueType::Text: return "text";
        case ValueType::FloatVec: return "floatvec";
        case ValueType::Positions: return "positions";
        case ValueType::Labels: return "labels";
        case ValueType::IntVec: return "intvec";
        case ValueType::Chem: return "chemdata";
        default: return "any";
    }
}

bool TypeFromName(const std::string& name, ValueType& out) {
    if (name == "float" || name == "number") out = ValueType::Float;
    else if (name == "int") out = ValueType::Int;
    else if (name == "text" || name == "string" || name == "str") out = ValueType::Text;
    else if (name == "floatvec" || name == "floats" || name == "array") out = ValueType::FloatVec;
    else if (name == "positions" || name == "coords") out = ValueType::Positions;
    else if (name == "labels") out = ValueType::Labels;
    else if (name == "intvec" || name == "ints" || name == "indices") out = ValueType::IntVec;
    else if (name == "chemdata" || name == "chem") out = ValueType::Chem;
    else if (name == "any") out = ValueType::Any;
    else return false;
    return true;
}

bool Compatible(ValueType from, ValueType to) {
    if (from == to || to == ValueType::Any || from == ValueType::Any) return true;
    if (from == ValueType::Int && to == ValueType::Float) return true;   // safe widening
    return false;
}

json ValueToJson(const Value& val) {
    switch (val.Type()) {
        case ValueType::Float: return std::get<double>(val.v);
        case ValueType::Int: return std::get<int64_t>(val.v);
        case ValueType::Text: return std::get<std::string>(val.v);
        case ValueType::FloatVec: return std::get<std::vector<double>>(val.v);
        case ValueType::Labels: return std::get<Labels>(val.v);
        case ValueType::IntVec: return std::get<std::vector<int64_t>>(val.v);
        case ValueType::Chem: return ChemicalDataToJson(std::get<ChemicalData>(val.v));
        case ValueType::Positions: {
            const Positions& p = std::get<Positions>(val.v);
            json rows = json::array();
            for (size_t i = 0; i < p.Count(); ++i)
                rows.push_back({p.xyz[3 * i], p.xyz[3 * i + 1], p.xyz[3 * i + 2]});
            return rows;
        }
        default: return nullptr;
    }
}

namespace {

// Infer a Value from arbitrary JSON (used for `any`-typed pins).
bool Infer(const json& j, Value& out, std::string& err) {
    if (j.is_number_integer()) { out = Value::I(j.get<int64_t>()); return true; }
    if (j.is_number()) { out = Value::F(j.get<double>()); return true; }
    if (j.is_string()) { out = Value::S(j.get<std::string>()); return true; }
    if (j.is_array()) {
        if (j.empty()) { out.v = std::vector<double>{}; return true; }
        if (j[0].is_string()) {
            Labels l;
            for (const auto& e : j) {
                if (!e.is_string()) { err = "mixed-type array"; return false; }
                l.push_back(e.get<std::string>());
            }
            out.v = std::move(l);
            return true;
        }
        if (j[0].is_array()) {
            std::string e2;
            return ValueFromJson(j, ValueType::Positions, out, e2) || (err = "unsupported nested array", false);
        }
        if (j[0].is_number()) {
            std::string e2;
            return ValueFromJson(j, ValueType::FloatVec, out, e2) || (err = e2, false);
        }
    }
    err = "unsupported JSON value";
    return false;
}

}  // namespace

bool ValueFromJson(const json& j, ValueType expected, Value& out, std::string& err) {
    switch (expected) {
        case ValueType::Any: return Infer(j, out, err);
        case ValueType::Float:
            if (!j.is_number()) { err = "expected a number"; return false; }
            out = Value::F(j.get<double>());
            return true;
        case ValueType::Int:
            if (!j.is_number_integer()) { err = "expected an integer"; return false; }
            out = Value::I(j.get<int64_t>());
            return true;
        case ValueType::Text:
            if (!j.is_string()) { err = "expected a string"; return false; }
            out = Value::S(j.get<std::string>());
            return true;
        case ValueType::FloatVec: {
            if (!j.is_array()) { err = "expected an array of numbers"; return false; }
            std::vector<double> a;
            a.reserve(j.size());
            for (const auto& e : j) {
                if (!e.is_number()) { err = "expected an array of numbers"; return false; }
                a.push_back(e.get<double>());
            }
            out.v = std::move(a);
            return true;
        }
        case ValueType::Positions: {
            if (!j.is_array()) { err = "expected an array of [x,y,z]"; return false; }
            Positions p;
            p.xyz.reserve(j.size() * 3);
            for (const auto& row : j) {
                if (!row.is_array() || row.size() != 3 || !row[0].is_number()) {
                    err = "expected an array of [x,y,z]";
                    return false;
                }
                for (int k = 0; k < 3; ++k) p.xyz.push_back(row[k].get<double>());
            }
            out.v = std::move(p);
            return true;
        }
        case ValueType::IntVec: {
            if (!j.is_array()) { err = "expected an array of integers"; return false; }
            std::vector<int64_t> a;
            a.reserve(j.size());
            for (const auto& e : j) {
                if (!e.is_number_integer()) { err = "expected an array of integers"; return false; }
                a.push_back(e.get<int64_t>());
            }
            out.v = std::move(a);
            return true;
        }
        case ValueType::Chem: {
            ChemicalData c;
            if (!ChemicalDataFromJson(j, c, err)) return false;
            out.v = std::move(c);
            return true;
        }
        case ValueType::Labels: {
            if (!j.is_array()) { err = "expected an array of strings"; return false; }
            Labels l;
            for (const auto& e : j) {
                if (!e.is_string()) { err = "expected an array of strings"; return false; }
                l.push_back(e.get<std::string>());
            }
            out.v = std::move(l);
            return true;
        }
    }
    err = "unhandled type";
    return false;
}

}  // namespace graph
