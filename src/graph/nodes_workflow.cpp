// The node types behind workflows: thin wrappers around native kernels
// (kernels_array.cpp, kernels_chem.cpp) plus the two app-bound ends of a
// workflow that a pure kernel cannot be -- reading the active frame (Chemical
// Data, in nodes_builtin.cpp) and writing a topology back onto it (Apply
// Topology, here). A node here is pins + a kernel id + the widgets for its
// parameters; the computation itself lives in the kernel table.

#include <algorithm>
#include <string>

#include <fmt/format.h>

#include "imgui.h"
#include "imgui_stdlib.h"

#include "app/actions.h"
#include "app/app_state.h"
#include "core/chemical_data.h"
#include "graph/executor.h"
#include "graph/graph.h"
#include "graph/node_registry.h"

namespace graph {

namespace {

std::string TextParam(const Node& n, const char* key, const std::string& fallback) {
    auto it = n.params.find(key);
    if (it == n.params.end()) return fallback;
    const std::string* t = it->second.AsText();
    return t ? *t : fallback;
}

double FloatParam(const Node& n, const char* key, double fallback) {
    auto it = n.params.find(key);
    double v = fallback;
    if (it == n.params.end() || !it->second.AsFloat(v)) return fallback;
    return v;
}

int64_t IntParam(const Node& n, const char* key, int64_t fallback) {
    auto it = n.params.find(key);
    int64_t v = fallback;
    if (it == n.params.end() || !it->second.AsInt(v)) return fallback;
    return v;
}

// A row of radio buttons choosing a text parameter. Returns true on change.
bool RadioParam(Node& n, const char* key, const char* fallback, std::initializer_list<const char*> options) {
    const std::string cur = TextParam(n, key, fallback);
    bool changed = false, first = true;
    for (const char* opt : options) {
        if (!first) ImGui::SameLine();
        first = false;
        if (ImGui::RadioButton(opt, cur == opt) && cur != opt) {
            n.params[key] = Value::S(opt);
            changed = true;
        }
    }
    return changed;
}

bool FloatWidget(Node& n, const char* key, const char* label, double fallback, float speed = 0.01f) {
    float v = (float)FloatParam(n, key, fallback);
    ImGui::PushItemWidth(100.0f);
    const bool changed = ImGui::DragFloat(label, &v, speed);
    ImGui::PopItemWidth();
    if (changed) n.params[key] = Value::F(v);
    return changed;
}

bool TextWidget(Node& n, const char* key, const char* label, const char* fallback) {
    std::string v = TextParam(n, key, fallback);
    ImGui::PushItemWidth(110.0f);
    const bool changed = ImGui::InputText(label, &v);
    ImGui::PopItemWidth();
    if (changed) n.params[key] = Value::S(v);
    return changed;
}

// Small "n entries" readout of an output array, so the graph shows sizes at a glance.
void SizeReadout(const Node& n, size_t pin, const char* what) {
    if (pin >= n.outValues.size() || n.outValues[pin].Empty()) return;
    const Value& v = n.outValues[pin];
    size_t count = 0;
    if (const auto* f = v.AsFloatVec()) count = f->size();
    else if (const auto* i = v.AsIntVec()) count = i->size();
    else return;
    ImGui::TextDisabled("%zu %s", count, what);
}

// ---- bodies ----

bool BodyVectorMath(AppState&, Node& n) {
    bool changed = RadioParam(n, "op", "add", {"add", "sub", "mul", "div", "min", "max"});
    changed |= FloatWidget(n, "b", "b (when pin free)", 0.0);
    SizeReadout(n, 0, "values");
    return changed;
}

bool BodyCompare(AppState&, Node& n) {
    bool changed = RadioParam(n, "op", "lt", {"<", "<=", ">", ">=", "==", "!="});
    changed |= FloatWidget(n, "b", "b (when pin free)", 0.0);
    if (n.outValues.size() > 1 && !n.outValues[1].Empty()) {
        int64_t c = 0;
        n.outValues[1].AsInt(c);
        size_t total = n.outValues[0].AsIntVec() ? n.outValues[0].AsIntVec()->size() : 0;
        ImGui::TextDisabled("%lld of %zu true", (long long)c, total);
    }
    return changed;
}

bool BodyReduce(AppState&, Node& n) {
    const bool changed = RadioParam(n, "op", "max", {"max", "min", "sum", "mean", "count"});
    if (!n.outValues.empty() && !n.outValues[0].Empty()) ImGui::TextDisabled("= %s", n.outValues[0].Preview().c_str());
    return changed;
}

bool BodyScalarMath(AppState&, Node& n) {
    bool changed = RadioParam(n, "op", "add", {"add", "sub", "mul", "div", "pow"});
    changed |= FloatWidget(n, "b", "b (when pin free)", 0.0);
    if (!n.outValues.empty() && !n.outValues[0].Empty()) ImGui::TextDisabled("= %s", n.outValues[0].Preview().c_str());
    return changed;
}

bool BodyGather(AppState&, Node& n) {
    SizeReadout(n, 0, "gathered");
    return false;
}

bool BodyFilter(AppState&, Node& n) {
    SizeReadout(n, 0, "kept");
    return false;
}

bool BodyGate(AppState&, Node& n) {
    bool inv = IntParam(n, "invert", 0) != 0;
    bool changed = false;
    if (ImGui::Checkbox("invert", &inv)) { n.params["invert"] = Value::I(inv ? 1 : 0); changed = true; }
    ImGui::SameLine();
    ImGui::TextDisabled(n.skipped ? "closed" : "open");
    return changed;
}

bool BodyCovalentRadii(AppState&, Node& n) {
    SizeReadout(n, 0, "radii");
    if (n.outValues.size() > 1 && !n.outValues[1].Empty()) {
        ImGui::SameLine();
        ImGui::TextDisabled("(max %s A)", n.outValues[1].Preview().c_str());
    }
    return false;
}

bool BodyNeighborList(AppState&, Node& n) {
    const bool changed = FloatWidget(n, "cutoff", "cutoff A (when pin free)", 2.0);
    SizeReadout(n, 0, "pairs");
    return changed;
}

bool BodyTopologyName(AppState&, Node& n) {
    const bool changed = TextWidget(n, "name", "topology", "bonds");
    if (n.outValues.size() > 2 && !n.outValues[2].Empty())
        ImGui::TextDisabled("%s pairs", n.outValues[2].Preview().c_str());
    return changed;
}

bool BodyWithTopology(AppState&, Node& n) {
    return TextWidget(n, "name", "topology", "bonds");
}

// ---- Apply Topology: write (i, j) as a topology of the active frame ----
// The sink of the covalent-bonds workflow: the renderer draws whatever the
// active frame's "bonds" topology holds, so this is what makes the result
// visible. App-bound by nature (it mutates AppState), hence an EvalFn.

std::string EvalApplyTopology(AppState& s, Node& n, const std::vector<const Value*>& in, std::vector<Value>& out) {
    if (!in[0] || !in[1]) return "inputs 'i' and 'j' must both be connected";
    const std::vector<int64_t>* i = in[0]->AsIntVec();
    const std::vector<int64_t>* j = in[1]->AsIntVec();
    if (!i || !j) return "'i' and 'j' must be int arrays";
    if (i->size() != j->size()) return fmt::format("'i' has {} entries, 'j' has {}", i->size(), j->size());
    ChemicalData* c = s.ActiveChem();
    if (!c) return "no structure loaded";
    const std::string name = TextParam(n, "name", "bonds");
    Topology& t = c->Topo(name);
    t.pairs.clear();
    t.pairs.reserve(i->size());
    for (size_t k = 0; k < i->size(); ++k) {
        const int64_t p = (*i)[k], q = (*j)[k];
        if (p < 0 || q < 0 || p >= (int64_t)c->natoms || q >= (int64_t)c->natoms)
            return fmt::format("pair {} ({}, {}) out of range for {} atoms", k, p, q, c->natoms);
        t.pairs.emplace_back((int32_t)std::min(p, q), (int32_t)std::max(p, q));
    }
    if (name == "bonds") MarkGeometryChanged(s);   // the renderer rebuilds its sticks
    out[0] = Value::I((int64_t)t.pairs.size());
    return "";
}

bool BodyApplyTopology(AppState&, Node& n) {
    const bool changed = TextWidget(n, "name", "topology", "bonds");
    if (!n.outValues.empty() && !n.outValues[0].Empty())
        ImGui::TextDisabled("wrote %s pairs to the active frame", n.outValues[0].Preview().c_str());
    else
        ImGui::TextDisabled("writes onto the active frame");
    return changed;
}

NodeTypeSpec Native(const char* id, const char* name, NodeKind kind, const char* category, const char* description,
                    std::vector<PinSpec> inputs, std::vector<PinSpec> outputs, const char* kernel, BodyFn body = nullptr) {
    NodeTypeSpec s;
    s.id = id;
    s.name = name;
    s.kind = kind;
    s.category = category;
    s.description = description;
    s.inputs = std::move(inputs);
    s.outputs = std::move(outputs);
    s.kernel = kernel;
    s.drawBody = std::move(body);
    return s;
}

}  // namespace

void RegisterWorkflowNodes(NodeTypeRegistry& r) {
    using VT = ValueType;
    // ---- arrays (analyze / Arrays) ----
    r.Register(Native("array.gather", "Gather", NodeKind::Analyze, "Arrays",
                      "values[indices]: pick entries of an array by index (per-atom data -> per-pair data).",
                      {{"values", VT::Any}, {"indices", VT::IntVec}}, {{"values", VT::Any}}, "array.gather", &BodyGather));
    r.Register(Native("array.math", "Vector Math", NodeKind::Analyze, "Arrays",
                      "Elementwise a (op) b; b is an array, a number, or the constant on the node.",
                      {{"a", VT::Any}, {"b", VT::Any}}, {{"values", VT::FloatVec}}, "array.math", &BodyVectorMath));
    r.Register(Native("array.compare", "Compare", NodeKind::Analyze, "Arrays",
                      "Elementwise a (cmp) b -> mask of 0/1, plus how many are true.",
                      {{"a", VT::Any}, {"b", VT::Any}}, {{"mask", VT::IntVec}, {"count", VT::Int}}, "array.compare", &BodyCompare));
    r.Register(Native("array.filter", "Filter", NodeKind::Analyze, "Arrays",
                      "The entries of an array where the mask is non-zero.",
                      {{"values", VT::Any}, {"mask", VT::IntVec}}, {{"values", VT::Any}}, "array.filter", &BodyFilter));
    r.Register(Native("array.reduce", "Reduce", NodeKind::Analyze, "Arrays",
                      "max / min / sum / mean / count of an array.",
                      {{"values", VT::Any}}, {{"value", VT::Float}}, "array.reduce", &BodyReduce));
    r.Register(Native("scalar.math", "Math", NodeKind::Analyze, "Arrays",
                      "a (op) b on two numbers (b from the pin, else the constant on the node).",
                      {{"a", VT::Float}, {"b", VT::Float}}, {{"value", VT::Float}}, "scalar.math", &BodyScalarMath));
    // ---- control flow (other / Flow) ----
    r.Register(Native("flow.gate", "Gate", NodeKind::Other, "Flow",
                      "If: passes 'value' through while 'pass' is non-zero; otherwise everything downstream is skipped.",
                      {{"pass", VT::Int}, {"value", VT::Any}}, {{"value", VT::Any}}, "flow.gate", &BodyGate));
    // ---- chemistry (analyze / Structure) ----
    r.Register(Native("chem.covalent_radii", "Covalent Radii", NodeKind::Analyze, "Structure",
                      "Per-atom covalent radii from the element table, and the largest of them.",
                      {{"chem", VT::Chem}}, {{"radii", VT::FloatVec}, {"max", VT::Float}}, "chem.covalent_radii", &BodyCovalentRadii));
    r.Register(Native("chem.vdw_radii", "VdW Radii", NodeKind::Analyze, "Structure",
                      "Per-atom van der Waals radii from the element table, and the largest of them.",
                      {{"chem", VT::Chem}}, {{"radii", VT::FloatVec}, {"max", VT::Float}}, "chem.vdw_radii", &BodyCovalentRadii));
    r.Register(Native("chem.neighbor_list", "Neighbor List", NodeKind::Analyze, "Structure",
                      "Cell-list neighbour search: every pair (i < j) closer than the cutoff, with distances.",
                      {{"chem", VT::Chem}, {"cutoff", VT::Float}},
                      {{"i", VT::IntVec}, {"j", VT::IntVec}, {"distances", VT::FloatVec}}, "chem.neighbor_list", &BodyNeighborList));
    r.Register(Native("chem.has_topology", "Has Topology", NodeKind::Analyze, "Structure",
                      "Whether the chemical data carries a named topology ('has'/'missing' drive a Gate).",
                      {{"chem", VT::Chem}}, {{"has", VT::Int}, {"missing", VT::Int}, {"count", VT::Int}}, "chem.has_topology", &BodyTopologyName));
    r.Register(Native("chem.topology_pairs", "Topology Pairs", NodeKind::Analyze, "Structure",
                      "The pairs of a named topology as two index arrays.",
                      {{"chem", VT::Chem}}, {{"i", VT::IntVec}, {"j", VT::IntVec}}, "chem.topology_pairs", &BodyWithTopology));
    r.Register(Native("chem.with_topology", "With Topology", NodeKind::Analyze, "Structure",
                      "A copy of the chemical data with a topology replaced by the (i, j) pairs.",
                      {{"chem", VT::Chem}, {"i", VT::IntVec}, {"j", VT::IntVec}}, {{"chem", VT::Chem}}, "chem.with_topology", &BodyWithTopology));
    // ---- sink (visualize / Render) ----
    r.Register({"core.apply_topology", "Apply Topology", NodeKind::Visualize, "Render",
                "Writes (i, j) pairs as a topology of the active frame; 'bonds' is what the 3D view draws.",
                {{"i", VT::IntVec}, {"j", VT::IntVec}},
                {{"count", VT::Int}},
                &EvalApplyTopology, &BodyApplyTopology});
}

}  // namespace graph
