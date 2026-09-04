// Nodes that talk to the loaded structures and the panels: Structure /
// Select Frame / Load Structure / Structure List, Render 3D (a 3D window of
// its own for whatever ChemicalData arrives) and Plot View (the 2D Plot
// panel's picker). Usable in any graph, including a scene graph.

#include <algorithm>
#include <filesystem>

#include <fmt/format.h>

#include "imgui.h"
#include "imgui_stdlib.h"
#if !defined(__EMSCRIPTEN__)
#include "portable-file-dialogs.h"
#endif

#include "app/actions.h"
#include "app/app_state.h"
#include "graph/graph_system.h"
#include "graph/node_registry.h"

namespace graph {

namespace {

constexpr int kStructureListPins = 6;

int64_t IntParam(const Node& n, const std::string& key, int64_t fallback) {
    auto it = n.params.find(key);
    int64_t v = fallback;
    if (it == n.params.end() || !it->second.AsInt(v)) return fallback;
    return v;
}

std::string TextParam(const Node& n, const std::string& key, const std::string& fallback = "") {
    auto it = n.params.find(key);
    if (it == n.params.end()) return fallback;
    const std::string* s = it->second.AsText();
    return s ? *s : fallback;
}

std::string AbsolutePath(const std::string& path) {
    std::error_code ec;
    const auto abs = std::filesystem::absolute(path, ec);
    return ec ? path : abs.lexically_normal().string();
}

StructureHandle HandleFor(const AppState& s, int index) {
    const Structure& st = s.structures[(size_t)index];
    StructureHandle h;
    h.name = st.name;
    h.path = st.path;
    h.index = index;
    h.frames = (int)st.frames.nframes;
    return h;
}

// Resolve a handle against the live structure list: by index when it still
// matches, else by path, else by name.
int ResolveHandle(const AppState& s, const StructureHandle& h) {
    if (h.index >= 0 && h.index < (int)s.structures.size()) {
        const Structure& st = s.structures[(size_t)h.index];
        if ((!h.path.empty() && st.path == h.path) || st.name == h.name) return h.index;
    }
    for (size_t i = 0; i < s.structures.size(); ++i)
        if (!h.path.empty() && s.structures[i].path == h.path) return (int)i;
    for (size_t i = 0; i < s.structures.size(); ++i)
        if (s.structures[i].name == h.name) return (int)i;
    return -1;
}

int FindStructureByPath(const AppState& s, const std::string& absPath) {
    for (size_t i = 0; i < s.structures.size(); ++i)
        if (!s.structures[i].path.empty() && AbsolutePath(s.structures[i].path) == absPath) return (int)i;
    return -1;
}

// ---- Structure: a handle to the active (or an indexed) loaded structure ----

std::string EvalStructure(AppState& s, Node& n, const std::vector<const Value*>&, std::vector<Value>& out) {
    const int64_t which = IntParam(n, "index", 0);   // 0 = active, else 1-based
    int index = s.activeStructure;
    if (which > 0) index = (int)which - 1;
    if (index < 0 || index >= (int)s.structures.size())
        return s.structures.empty() ? "no structure loaded" : fmt::format("no structure #{}", which);
    out[0].v = HandleFor(s, index);
    return "";
}

bool BodyStructure(AppState& s, Node& n) {
    bool changed = false;
    int which = (int)IntParam(n, "index", 0);
    ImGui::PushItemWidth(90.0f);
    if (ImGui::InputInt("index (0 = active)", &which)) {
        n.params["index"] = Value::I(std::clamp(which, 0, (int)s.structures.size()));
        changed = true;
    }
    ImGui::PopItemWidth();
    const int index = which > 0 ? which - 1 : s.activeStructure;
    if (index >= 0 && index < (int)s.structures.size())
        ImGui::TextDisabled("%s (%u frames)", s.structures[(size_t)index].name.c_str(), s.structures[(size_t)index].frames.nframes);
    else
        ImGui::TextDisabled("no structure");
    return changed;
}

// ---- Select Frame: one frame of a structure as ChemicalData ----

std::string EvalSelectFrame(AppState& s, Node& n, const std::vector<const Value*>& in, std::vector<Value>& out) {
    if (!in[0]) return "input 'structure' not connected";
    const StructureHandle* h = in[0]->AsStructure();
    if (!h) return "wrong input type on 'structure'";
    const int index = ResolveHandle(s, *h);
    if (index < 0) return fmt::format("structure '{}' is no longer loaded", h->name);
    Structure& st = s.structures[(size_t)index];
    if (st.frames.nframes == 0) return "structure has no frames";
    int frame = st.activeFrame;
    if (IntParam(n, "follow", 1) == 0) frame = (int)IntParam(n, "frame", 1) - 1;
    if (frame < 0 || frame >= (int)st.frames.nframes)
        return fmt::format("frame {} out of range 1..{}", frame + 1, st.frames.nframes);
    ChemicalData c = st.frames.data[(size_t)frame];
    n.params["_label"] = Value::S(st.frames.nframes > 1 ? fmt::format("{}  |  frame {}/{}", st.name, frame + 1, st.frames.nframes)
                                                        : st.name);
    out[0].v = std::move(c);
    out[1] = Value::I(frame);
    return "";
}

bool BodySelectFrame(AppState&, Node& n) {
    bool changed = false;
    bool follow = IntParam(n, "follow", 1) != 0;
    if (ImGui::Checkbox("follow active frame", &follow)) {
        n.params["follow"] = Value::I(follow ? 1 : 0);
        changed = true;
    }
    if (!follow) {
        int frame = (int)IntParam(n, "frame", 1);
        ImGui::PushItemWidth(90.0f);
        if (ImGui::InputInt("frame", &frame)) {
            n.params["frame"] = Value::I(std::max(frame, 1));
            changed = true;
        }
        ImGui::PopItemWidth();
    }
    return changed;
}

// ---- Render 3D: draw ChemicalData in a 3D window of the node's own ----

std::string EvalRender3D(AppState& s, Node& n, const std::vector<const Value*>& in, std::vector<Value>&) {
    GraphSystem& gs = s.GraphSys();
    if (!in[0]) return "input 'chem' not connected";
    const ChemicalData* chem = in[0]->AsChem();
    if (!chem) return "wrong input type on 'chem'";
    if (std::string v = chem->Validate(); !v.empty()) return v;
    ChemicalData atoms = *chem;
    // Anything producing ChemicalData without bonds -- a script, an analysis
    // -- still gets drawn with perceived bonds.
    if (!atoms.FindTopology("bonds")) PerceiveBonds(atoms, s.calc.bondTolerance);
    // The label travels out-of-band: the Select Frame node upstream knows
    // which structure/frame this is, the ChemicalData does not.
    std::string label;
    if (const Graph* g = OwningGraph(gs, n))
        if (const Link* l = g->LinkInto(n.id, 0))
            if (const Node* up = g->FindNode(l->fromNode)) label = TextParam(*up, "_label");
    if (label.empty()) label = fmt::format("{} atoms", atoms.natoms);

    NodeView& view = gs.ViewFor(n, NodeViewKind::View3D);
    view.atoms = std::move(atoms);
    view.label = std::move(label);
    ++view.version;
    return "";
}

bool BodyRender3D(AppState& s, Node& n) {
    // The render settings are shared app state (the Controls panel, the view
    // toolbar and the `style`/`set` commands edit the same values), so the
    // node edits them directly rather than keeping a copy in its params.
    bool changed = false;
    NodeView* view = s.GraphSys().FindView(n.uid);
    if (!view) {
        ImGui::TextDisabled("(run the graph to open its 3D window)");
    } else if (view->open) {
        ImGui::TextDisabled("shown in its own 3D window");
    } else if (ImGui::SmallButton("Show 3D window")) {
        view->open = true;
    }
    const char* names[] = {"ball-and-stick", "spheres", "sticks"};
    for (int i = 0; i < 3; ++i) {
        if (i) ImGui::SameLine();
        if (ImGui::RadioButton(names[i], (int)s.render.style == i) && (int)s.render.style != i) {
            s.render.style = (RenderStyle)i;
            MarkGeometryChanged(s);
            changed = true;
        }
    }
    ImGui::PushItemWidth(110.0f);
    if (s.render.style == RenderStyle::Spheres) {
        if (ImGui::SliderFloat("sphere scale", &s.render.sphereScale, 0.2f, 1.5f)) { MarkGeometryChanged(s); changed = true; }
    } else {
        if (s.render.style == RenderStyle::BallAndStick &&
            ImGui::SliderFloat("ball scale", &s.render.ballScale, 0.05f, 1.0f)) { MarkGeometryChanged(s); changed = true; }
        if (ImGui::SliderFloat("stick radius", &s.render.stickRadius, 0.05f, 0.6f)) { MarkGeometryChanged(s); changed = true; }
    }
    ImGui::PopItemWidth();
    if (ImGui::Checkbox("grid", &s.drawGrid)) changed = true;
    ImGui::SameLine();
    if (ImGui::Checkbox("atom numbers", &s.drawAtomNumbers)) changed = true;
    if (view) ImGui::TextDisabled("drawing %u atoms, %zu bonds", view->atoms.natoms, view->atoms.BondCount());
    else ImGui::TextDisabled("(nothing to draw)");
    return changed;
}

// ---- Load Structure: read a file into the structure list ----

std::string EvalLoadStructure(AppState& s, Node& n, const std::vector<const Value*>&, std::vector<Value>& out) {
    const std::string path = TextParam(n, "path");
    if (path.empty()) return "no file chosen";
    const std::string abs = AbsolutePath(path);
    int index = FindStructureByPath(s, abs);
    if (index < 0) {
        // Not loaded yet: load it.
        CommandResult r = LoadStructureFile(s, path, s.structures.empty());
        if (!r.ok) return r.message;
        index = FindStructureByPath(s, abs);
        if (index < 0) return "loaded, but cannot find the structure by path";
    }
    out[0].v = HandleFor(s, index);
    return "";
}

bool BodyLoadStructure(AppState&, Node& n) {
    bool changed = false;
    std::string path = TextParam(n, "path");
    ImGui::Text("%s", path.empty() ? "<no file>" : std::filesystem::path(path).filename().string().c_str());
#if !defined(__EMSCRIPTEN__)
    if (ImGui::SmallButton("Browse...")) {
        auto sel = pfd::open_file("Open geometry", ".", {"Geometry files", "*.xyz", "All files", "*"}).result();
        if (!sel.empty()) {
            n.params["path"] = Value::S(sel.front());
            changed = true;
        }
    }
#endif
    ImGui::PushItemWidth(200.0f);
    if (ImGui::InputText("path", &path)) { n.params["path"] = Value::S(path); changed = true; }
    ImGui::PopItemWidth();
    return changed;
}

// ---- Structure List: the loaded structures; exactly one is active ----

std::string EvalStructureList(AppState& s, Node&, const std::vector<const Value*>& in, std::vector<Value>& out) {
    int wired = 0;
    for (int k = 0; k < kStructureListPins; ++k) {
        if (!in[k]) continue;
        if (!in[k]->AsStructure()) return fmt::format("wrong input type on 's{}'", k + 1);
        ++wired;
    }
    (void)wired;
    if (s.activeStructure >= 0 && s.activeStructure < (int)s.structures.size()) out[0].v = HandleFor(s, s.activeStructure);
    return "";
}

bool BodyStructureList(AppState& s, Node&) {
    if (s.structures.empty()) {
        ImGui::TextDisabled("nothing loaded");
        return false;
    }
    for (size_t i = 0; i < s.structures.size(); ++i) {
        const Structure& st = s.structures[i];
        const bool active = (int)i == s.activeStructure;
        ImGui::PushID((int)i);
        if (ImGui::RadioButton(fmt::format("{} ({} frame{})", st.name, st.frames.nframes, st.frames.nframes == 1 ? "" : "s").c_str(), active) && !active)
            SetActiveStructure(s, (int)i);
        ImGui::PopID();
    }
    return false;   // activating a structure changes app state, not the graph
}

// ---- Plot View: which published/built-in plot the 2D Plot panel shows ----

std::string EvalPlotView(AppState&, Node&, const std::vector<const Value*>&, std::vector<Value>&) { return ""; }

bool BodyPlotView(AppState& s, Node&) {
    const char* builtin[] = {"Energy per frame", "Measurements per frame"};
    for (int i = 0; i < AppState::kBuiltinPlotCount; ++i)
        if (ImGui::RadioButton(builtin[i], s.twoDPlotIndex == i)) s.twoDPlotIndex = i;
    if (!s.plots.empty()) ImGui::Separator();
    for (size_t i = 0; i < s.plots.size(); ++i) {
        const int idx = AppState::kBuiltinPlotCount + (int)i;
        if (ImGui::RadioButton(s.plots[i].name.c_str(), s.twoDPlotIndex == idx)) s.twoDPlotIndex = idx;
    }
    ImGui::TextDisabled("Plot 2D nodes (any graph) publish here");
    return false;
}

}  // namespace

void RegisterViewNodes(NodeTypeRegistry& r) {
    r.Register({"view.structure", "Structure", NodeKind::Build, "View",
                "The active structure (or one by index) as a handle to its frames.",
                {},
                {{"structure", ValueType::Structure}},
                &EvalStructure, &BodyStructure});
    r.Register({"view.select_frame", "Select Frame", NodeKind::Build, "View",
                "One frame of a structure (the active frame, or a fixed one) as ChemicalData.",
                {{"structure", ValueType::Structure}},
                {{"chem", ValueType::Chem}, {"frame", ValueType::Int}},
                &EvalSelectFrame, &BodySelectFrame});
    r.Register({"view.render3d", "Render 3D", NodeKind::Visualize, "View",
                "Draws the incoming ChemicalData in a 3D window of its own (style, grid, ...).",
                {{"chem", ValueType::Chem}},
                {},
                &EvalRender3D, &BodyRender3D});
    r.Register({"view.load_structure", "Load Structure", NodeKind::Build, "View",
                "Reads an xyz file into the structure list (when the graph runs).",
                {},
                {{"structure", ValueType::Structure}},
                &EvalLoadStructure, &BodyLoadStructure});
    std::vector<PinSpec> listPins;
    for (int k = 0; k < kStructureListPins; ++k) listPins.push_back({fmt::format("s{}", k + 1), ValueType::Structure});
    r.Register({"view.structure_list", "Structure List", NodeKind::Other, "View",
                "The loaded structures; pick the active one.",
                listPins,
                {{"active", ValueType::Structure}},
                &EvalStructureList, &BodyStructureList});
    r.Register({"view.plot_view", "Plot View", NodeKind::Visualize, "View",
                "Which plot the 2D Plot panel shows: a built-in per-frame plot or a published one.",
                {},
                {},
                &EvalPlotView, &BodyPlotView});
}

}  // namespace graph
