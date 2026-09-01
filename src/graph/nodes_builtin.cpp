// ChemLab's built-in node types. Each is a NodeTypeSpec: pins, an evaluate
// function (UI-free) and optionally widgets drawn in the node body. Adding a
// core node means adding one entry here (or a Register... call elsewhere).

#include <algorithm>

#include <fmt/format.h>

#include "imgui.h"

#include "app/app_state.h"
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
    r.Register({"core.watch", "Watch", "Output",
                "Shows the value arriving at its input.",
                {{"value", ValueType::Any}},
                {},
                &EvalWatch, &BodyWatch});
}

}  // namespace graph
