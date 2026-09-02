// ChemLab's built-in node types. Each is a NodeTypeSpec: pins, an evaluate
// function (UI-free) and optionally widgets drawn in the node body. Adding a
// core node means adding one entry here (or a Register... call elsewhere).

#include <algorithm>
#include <cmath>

#include <fmt/format.h>

#include "imgui.h"

#include "app/actions.h"
#include "app/app_state.h"
#include "graph/chem_convert.h"
#include "graph/chemical_data.h"
#include "graph/graph.h"
#include "graph/node_registry.h"

namespace graph {

namespace {

int64_t IntParam(const Node& n, const std::string& key, int64_t fallback) {
    auto it = n.params.find(key);
    int64_t v = fallback;
    if (it == n.params.end() || !it->second.AsInt(v)) return fallback;
    return v;
}

// ---- Active Frame: positions + labels of the active structure's frame ----

std::string EvalActiveFrame(AppState& s, Node&, const std::vector<const Value*>&, std::vector<Value>& out) {
    const Atoms* a = s.ActiveAtoms();
    if (!a) return "no structure loaded";
    Positions p;
    p.xyz.reserve((size_t)a->natoms * 3);
    for (const Vector3& r : a->xyz) {
        p.xyz.push_back(r.x);
        p.xyz.push_back(r.y);
        p.xyz.push_back(r.z);
    }
    out[0].v = std::move(p);
    out[1].v = a->labels;
    return "";
}

bool BodyActiveFrame(AppState& s, Node&) {
    const Atoms* a = s.ActiveAtoms();
    if (a)
        ImGui::TextDisabled("%u atoms, frame %d/%d", a->natoms, s.ActiveFrameIndex() + 1, s.FrameCount());
    else
        ImGui::TextDisabled("no structure");
    return false;
}

// ---- Atom Pair: two atom indices (1-based in the UI, 0-based on the pins) ----

std::string EvalAtomPair(AppState& s, Node& n, const std::vector<const Value*>&, std::vector<Value>& out) {
    const int64_t i = IntParam(n, "i", 1), j = IntParam(n, "j", 2);
    if (i < 1 || j < 1) return "atom indices are 1-based";
    if (const Atoms* a = s.ActiveAtoms()) {
        if (i > a->natoms || j > a->natoms)
            return fmt::format("index out of range (structure has {} atoms)", a->natoms);
    }
    out[0] = Value::I(i - 1);
    out[1] = Value::I(j - 1);
    return "";
}

bool BodyAtomPair(AppState& s, Node& n) {
    bool changed = false;
    int i = (int)IntParam(n, "i", 1), j = (int)IntParam(n, "j", 2);
    ImGui::PushItemWidth(90.0f);
    if (ImGui::InputInt("atom i", &i)) { n.params["i"] = Value::I(std::max(i, 1)); changed = true; }
    if (ImGui::InputInt("atom j", &j)) { n.params["j"] = Value::I(std::max(j, 1)); changed = true; }
    ImGui::PopItemWidth();
    if (s.selected.size() == 2) {
        if (ImGui::SmallButton("from selection")) {
            auto it = s.selected.begin();
            n.params["i"] = Value::I(*it++ + 1);
            n.params["j"] = Value::I(*it + 1);
            changed = true;
        }
    } else {
        ImGui::TextDisabled("(select 2 atoms to grab)");
    }
    return changed;
}

// ---- Chemical Data: the active frame as a ChemicalData object ----

std::string EvalChemicalData(AppState& s, Node&, const std::vector<const Value*>&, std::vector<Value>& out) {
    const Atoms* a = s.ActiveAtoms();
    if (!a) return "no structure loaded";
    ChemicalData c;
    std::string err;
    if (!AtomsToChemicalData(*a, c, err)) return err;
    out[0].v = std::move(c);
    return "";
}

bool BodyChemicalData(AppState& s, Node&) {
    if (const Atoms* a = s.ActiveAtoms())
        ImGui::TextDisabled("%u atoms, %zu bonds", a->natoms, a->covalentBondList.pairs.size());
    else
        ImGui::TextDisabled("no structure");
    return false;
}

// ---- Selected Atoms: the current selection as indices ----

std::string EvalSelectedAtoms(AppState& s, Node&, const std::vector<const Value*>&, std::vector<Value>& out) {
    std::vector<int64_t> idx(s.selected.begin(), s.selected.end());   // set<int>: already sorted, 0-based
    out[0].v = std::move(idx);
    return "";
}

bool BodySelectedAtoms(AppState& s, Node&) {
    ImGui::TextDisabled("%zu atom%s selected", s.selected.size(), s.selected.size() == 1 ? "" : "s");
    return false;
}

// ---- Float: a constant, settable on the node ----

double FloatParam(const Node& n, const std::string& key, double fallback) {
    auto it = n.params.find(key);
    double v = fallback;
    if (it == n.params.end() || !it->second.AsFloat(v)) return fallback;
    return v;
}

std::string EvalFloat(AppState&, Node& n, const std::vector<const Value*>&, std::vector<Value>& out) {
    out[0] = Value::F(FloatParam(n, "value", 0.0));
    return "";
}

bool BodyFloat(AppState&, Node& n) {
    float v = (float)FloatParam(n, "value", 0.0);
    ImGui::PushItemWidth(110.0f);
    const bool changed = ImGui::DragFloat("value", &v, 0.01f);
    ImGui::PopItemWidth();
    if (changed) n.params["value"] = Value::F(v);
    return changed;
}

// ---- Highlight Alpha: full alpha on chosen atoms, dim everywhere else ----

std::string EvalHighlightAlpha(AppState&, Node& n, const std::vector<const Value*>& in, std::vector<Value>& out) {
    if (!in[0]) return "input 'chem' not connected";
    if (!in[1]) return "input 'indices' not connected";
    const ChemicalData* chem = in[0]->AsChem();
    const std::vector<int64_t>* idx = in[1]->AsIntVec();
    if (!chem || !idx) return "wrong input types";
    double dim = FloatParam(n, "alpha", 0.2);
    if (in[2] && !in[2]->AsFloat(dim)) return "input 'alpha' is not a number";
    dim = std::clamp(dim, 0.0, 1.0);
    std::vector<double> alphas((size_t)chem->natoms, dim);
    for (const int64_t i : *idx) {
        if (i < 0 || i >= (int64_t)chem->natoms)
            return fmt::format("index {} out of range 0..{}", i, (int)chem->natoms - 1);
        alphas[(size_t)i] = 1.0;
    }
    out[0].v = std::move(alphas);
    return "";
}

bool BodyHighlightAlpha(AppState&, Node& n) {
    float v = (float)FloatParam(n, "alpha", 0.2);
    ImGui::PushItemWidth(110.0f);
    const bool changed = ImGui::SliderFloat("dim alpha", &v, 0.0f, 1.0f);
    ImGui::PopItemWidth();
    if (changed) n.params["alpha"] = Value::F(v);
    ImGui::TextDisabled("(used when 'alpha' pin is free)");
    return changed;
}

// ---- Apply Atom Alpha: write per-atom alphas into the renderer ----

std::string EvalApplyAtomAlpha(AppState& s, Node&, const std::vector<const Value*>& in, std::vector<Value>&) {
    if (!in[0]) return "input 'alphas' not connected";
    const std::vector<double>* alphas = in[0]->AsFloatVec();
    if (!alphas) return "wrong input type";
    if (s.modelDirty) RebuildModel(s);
    if (!s.model.IsLoaded()) return "no structure loaded";
    if (alphas->size() != s.model.AtomCount())
        return fmt::format("got {} alphas for {} atoms", alphas->size(), s.model.AtomCount());
    for (size_t i = 0; i < alphas->size(); ++i)
        s.model.atomColors[i].a = (unsigned char)std::lround(255.0 * std::clamp((*alphas)[i], 0.0, 1.0));
    return "";
}

bool BodyApplyAtomAlpha(AppState&, Node&) {
    ImGui::TextDisabled("writes atom alpha in the 3D view");
    return false;
}

// ---- Atom Index: one atom index (1-based in the UI, 0-based on the pin) ----

std::string EvalAtomIndex(AppState& s, Node& n, const std::vector<const Value*>&, std::vector<Value>& out) {
    const int64_t i = IntParam(n, "i", 1);
    if (i < 1) return "atom index is 1-based";
    if (const Atoms* a = s.ActiveAtoms())
        if (i > (int64_t)a->natoms) return fmt::format("index out of range (structure has {} atoms)", a->natoms);
    out[0] = Value::I(i - 1);
    return "";
}

bool BodyAtomIndex(AppState& s, Node& n) {
    bool changed = false;
    int i = (int)IntParam(n, "i", 1);
    ImGui::PushItemWidth(90.0f);
    if (ImGui::InputInt("atom", &i)) { n.params["i"] = Value::I(std::max(i, 1)); changed = true; }
    ImGui::PopItemWidth();
    if (!s.selected.empty() && ImGui::SmallButton("from selection")) {
        n.params["i"] = Value::I(*s.selected.begin() + 1);
        changed = true;
    }
    return changed;
}

// ---- Bonded Atoms: neighbors of an atom in one of the chemdata topologies ----

std::string EvalBondedAtoms(AppState&, Node& n, const std::vector<const Value*>& in, std::vector<Value>& out) {
    if (!in[0]) return "input 'chem' not connected";
    if (!in[1]) return "input 'i' not connected";
    const ChemicalData* chem = in[0]->AsChem();
    if (!chem) return "wrong input type on 'chem'";
    int64_t i = 0;
    if (!in[1]->AsInt(i)) return "input 'i' is not an integer";
    if (i < 0 || i >= (int64_t)chem->natoms)
        return fmt::format("index {} out of range 0..{}", i, (int)chem->natoms - 1);
    const std::string* topoName = n.params.count("topology") ? n.params.at("topology").AsText() : nullptr;
    const std::string topo = topoName ? *topoName : "bonds";
    const Topology* t = chem->FindTopology(topo);
    if (!t) return fmt::format("chemdata has no topology '{}'", topo);
    std::vector<int64_t> idx;
    if (IntParam(n, "include_self", 1) != 0) idx.push_back(i);
    for (const auto& [a, b] : t->pairs) {
        if (a == (int32_t)i) idx.push_back(b);
        if (b == (int32_t)i) idx.push_back(a);
    }
    std::sort(idx.begin(), idx.end());
    idx.erase(std::unique(idx.begin(), idx.end()), idx.end());
    out[0].v = std::move(idx);
    return "";
}

bool BodyBondedAtoms(AppState&, Node& n) {
    bool changed = false;
    bool self = IntParam(n, "include_self", 1) != 0;
    if (ImGui::Checkbox("include atom itself", &self)) {
        n.params["include_self"] = Value::I(self ? 1 : 0);
        changed = true;
    }
    ImGui::TextDisabled("topology: bonds");
    return changed;
}

// ---- Watch: displays whatever value reaches it ----

std::string EvalWatch(AppState&, Node& n, const std::vector<const Value*>& in, std::vector<Value>&) {
    if (!in[0]) {
        n.params.erase("last");
        return "input not connected";
    }
    n.params["last"] = *in[0];
    return "";
}

bool BodyWatch(AppState&, Node& n) {
    auto it = n.params.find("last");
    if (it == n.params.end()) {
        ImGui::TextDisabled("(run the graph)");
    } else {
        ImGui::Text("%s", it->second.Preview().c_str());
        ImGui::TextDisabled("%s", TypeName(it->second.Type()));
    }
    return false;
}

}  // namespace

void RegisterBuiltinNodes(NodeTypeRegistry& r) {
    r.Register({"core.active_frame", "Active Frame", "Sources",
                "Positions and labels of the active structure's current frame.",
                {},
                {{"positions", ValueType::Positions}, {"labels", ValueType::Labels}},
                &EvalActiveFrame, &BodyActiveFrame});
    r.Register({"core.atom_pair", "Atom Pair", "Sources",
                "Two atom indices (1-based here, 0-based on the output pins).",
                {},
                {{"i", ValueType::Int}, {"j", ValueType::Int}},
                &EvalAtomPair, &BodyAtomPair});
    r.Register({"core.chemical_data", "Chemical Data", "Sources",
                "Active frame as a ChemicalData object (R, Z, bonds topology).",
                {},
                {{"chem", ValueType::Chem}},
                &EvalChemicalData, &BodyChemicalData});
    r.Register({"core.selected_atoms", "Selected Atoms", "Sources",
                "The current selection as 0-based atom indices.",
                {},
                {{"indices", ValueType::IntVec}},
                &EvalSelectedAtoms, &BodySelectedAtoms});
    r.Register({"core.float", "Float", "Sources",
                "A constant number, set on the node.",
                {},
                {{"value", ValueType::Float}},
                &EvalFloat, &BodyFloat});
    r.Register({"core.atom_index", "Atom Index", "Sources",
                "One atom index (1-based here, 0-based on the output pin).",
                {},
                {{"i", ValueType::Int}},
                &EvalAtomIndex, &BodyAtomIndex});
    r.Register({"core.bonded_atoms", "Bonded Atoms", "Compute",
                "Atoms bonded to a given atom, from a chemdata topology.",
                {{"chem", ValueType::Chem}, {"i", ValueType::Int}},
                {{"indices", ValueType::IntVec}},
                &EvalBondedAtoms, &BodyBondedAtoms});
    r.Register({"core.highlight_alpha", "Highlight Alpha", "Compute",
                "Per-atom alphas: 1.0 for the given indices, a dim alpha elsewhere.",
                {{"chem", ValueType::Chem}, {"indices", ValueType::IntVec}, {"alpha", ValueType::Float}},
                {{"alphas", ValueType::FloatVec}},
                &EvalHighlightAlpha, &BodyHighlightAlpha});
    r.Register({"core.apply_atom_alpha", "Apply Atom Alpha", "Render",
                "Applies per-atom alphas to the 3D view (runs when the graph runs).",
                {{"alphas", ValueType::FloatVec}},
                {},
                &EvalApplyAtomAlpha, &BodyApplyAtomAlpha});
    r.Register({"core.watch", "Watch", "Output",
                "Shows the value arriving at its input.",
                {{"value", ValueType::Any}},
                {},
                &EvalWatch, &BodyWatch});
}

}  // namespace graph
