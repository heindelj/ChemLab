// The nodes behind the panels, plus the panel-graph plumbing: every panel
// owns a small graph (GraphSystem::panelGraphs) that it evaluates before
// drawing. This file has the node types those graphs are made of, the
// default graph for each panel (SeedPanelGraph), the hooks that keep the
// Active Structure graph in step with files loaded elsewhere, and the
// change-detection that makes evaluating panel graphs every frame cheap.
//
//   Structure View     Structure -> Select Frame -> Render 3D
//   Active Structure   Load Structure x N -> Structure List
//   2D Plot            Plot View (picks among the published plots)
//   everything else    one "panel.<id>" wrapper node (registered by the UI)

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
#include "graph/chem_convert.h"
#include "graph/graph_system.h"
#include "graph/node_registry.h"

namespace graph {

namespace {

constexpr const char* kStructureViewPanel = "structure_view";
constexpr const char* kActiveStructurePanel = "active_structure";
constexpr const char* kPlotPanel = "plot_2d";
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

// The graph a node lives in (nodes are stable in their deque, so compare addresses).
const Graph* OwningGraph(const GraphSystem& gs, const Node& n) {
    auto contains = [&](const Graph& g) {
        for (const Node& m : g.nodes)
            if (&m == &n) return true;
        return false;
    };
    if (contains(gs.graph)) return &gs.graph;
    for (const auto& [id, pg] : gs.panelGraphs)
        if (contains(pg.graph)) return &pg.graph;
    return nullptr;
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
    ChemicalData c;
    std::string err;
    if (!AtomsToChemicalData(st.frames.atoms[(size_t)frame], c, err)) return err;
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

// ---- Render 3D: hand ChemicalData to the Structure View ----

std::string EvalRender3D(AppState& s, Node& n, const std::vector<const Value*>& in, std::vector<Value>&) {
    if (!in[0]) return "input 'chem' not connected";
    const ChemicalData* chem = in[0]->AsChem();
    if (!chem) return "wrong input type on 'chem'";
    View3DRequest& req = s.GraphSys().view3d;
    std::string err;
    Atoms atoms;
    if (!ChemicalDataToAtoms(*chem, atoms, err, s.calc.bondTolerance)) return err;
    req.atoms = std::move(atoms);
    req.valid = true;
    // The label travels out-of-band: the Select Frame node upstream knows
    // which structure/frame this is, the ChemicalData does not.
    req.label.clear();
    if (const Graph* g = OwningGraph(s.GraphSys(), n))
        if (const Link* l = g->LinkInto(n.id, 0))
            if (const Node* up = g->FindNode(l->fromNode)) req.label = TextParam(*up, "_label");
    if (req.label.empty()) req.label = fmt::format("{} atoms", req.atoms.natoms);
    s.modelDirty = true;
    return "";
}

bool BodyRender3D(AppState& s, Node&) {
    // The render settings are shared app state (the Controls panel, the view
    // toolbar and the `style`/`set` commands edit the same values), so the
    // node edits them directly rather than keeping a copy in its params.
    bool changed = false;
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
    const View3DRequest& req = s.GraphSys().view3d;
    if (req.valid) ImGui::TextDisabled("drawing %u atoms, %zu bonds", req.atoms.natoms, req.atoms.covalentBondList.pairs.size());
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
        // Not loaded yet (a node the user added by hand): load it. This calls
        // back into OnStructureLoaded, which finds this node by path and adds
        // nothing.
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

// ---- panel graph helpers ----

std::string LinkByName(Graph& g, Node& from, const char* out, Node& to, const char* in) {
    const int o = FindOutputPin(from, out), i = FindInputPin(to, in);
    if (o < 0 || i < 0) return fmt::format("missing pin {} -> {}", out, in);
    std::string err;
    if (!g.AddLink(from.id, o, to.id, i, &err)) return err;
    return "";
}

Node* FindNodeOfType(Graph& g, const std::string& typeId) {
    for (Node& n : g.nodes)
        if (n.typeId == typeId) return &n;
    return nullptr;
}

Node* FindLoadNode(Graph& g, const std::string& absPath) {
    for (Node& n : g.nodes)
        if (n.typeId == "view.load_structure" && AbsolutePath(TextParam(n, "path")) == absPath) return &n;
    return nullptr;
}

// Add a Load Structure node for structures[index] and wire it into the first
// free pin of the Structure List node.
void AddLoadNode(AppState& s, Graph& g, int index) {
    const Structure& st = s.structures[(size_t)index];
    if (st.path.empty()) return;   // created in-app: nothing to load from
    int loads = 0;
    for (const Node& n : g.nodes) loads += n.typeId == "view.load_structure";
    Node* load = g.AddNode("view.load_structure", 40.0f, 60.0f + 175.0f * (float)loads);
    if (!load) return;
    load->params["path"] = Value::S(st.path);
    if (Node* list = FindNodeOfType(g, "view.structure_list")) {
        for (int k = 0; k < kStructureListPins; ++k) {
            if (g.LinkInto(list->id, k)) continue;
            std::string err;
            g.AddLink(load->id, 0, list->id, k, &err);
            break;
        }
    }
}

// Fingerprint of everything the panel graphs read from the app, so a graph
// is only re-evaluated when something it could depend on changed.
uint64_t PanelStamp(const AppState& s, const Graph& g) {
    uint64_t h = 1469598103934665603ull;
    auto mix = [&](uint64_t v) { h = (h ^ v) * 1099511628211ull; };
    mix(g.version);
    mix(s.structures.size());
    mix((uint64_t)(int64_t)s.activeStructure);
    for (const Structure& st : s.structures) {
        mix((uint64_t)(int64_t)st.activeFrame);
        mix(st.frames.dataVersion);
        mix(st.frames.nframes);
    }
    mix(s.selected.size());
    mix(s.measurementsVersion);
    mix(s.plots.size());
    return h;
}

}  // namespace

// ---------------------------------------------------------------------------
// GraphSystem: panel graphs
// ---------------------------------------------------------------------------
PanelGraph& GraphSystem::Panel(AppState& state, const std::string& panelId) {
    auto it = panelGraphs.find(panelId);
    if (it != panelGraphs.end()) return it->second;
    PanelGraph& pg = panelGraphs[panelId];
    SeedPanelGraph(state, panelId, pg.graph);
    return pg;
}

std::string GraphSystem::RunPanel(AppState& state, const std::string& panelId, bool force) {
    PanelGraph& pg = Panel(state, panelId);
    const uint64_t stamp = PanelStamp(state, pg.graph);
    if (!force && stamp == pg.lastStamp) return pg.lastError;
    const bool hadView = view3d.valid;
    // The Structure View's graph owns the 3D request: if its Render 3D node
    // is gone, the view falls back to the active frame.
    if (panelId == kStructureViewPanel) view3d.valid = false;
    pg.lastError = pg.graph.Evaluate(state, store, panelId + "/");
    // Evaluation may itself change the state it fingerprints (a Load
    // Structure node loading a file); fingerprint afterwards so that does
    // not trigger a second run.
    pg.lastStamp = PanelStamp(state, pg.graph);
    ++pg.runCount;
    if (panelId == kStructureViewPanel && hadView != view3d.valid) state.modelDirty = true;
    return pg.lastError;
}

void GraphSystem::ResetPanel(AppState& state, const std::string& panelId) {
    PanelGraph& pg = panelGraphs[panelId];
    pg.graph.Clear();
    pg.lastStamp = ~0ull;
    pg.lastError.clear();
    SeedPanelGraph(state, panelId, pg.graph);
}

void SeedPanelGraph(AppState& state, const std::string& panelId, Graph& g) {
    if (panelId == kStructureViewPanel) {
        Node* src = g.AddNode("view.structure", 40, 60);
        Node* frame = g.AddNode("view.select_frame", 320, 60);
        Node* render = g.AddNode("view.render3d", 600, 60);
        if (src && frame && render) {
            LinkByName(g, *src, "structure", *frame, "structure");
            LinkByName(g, *frame, "chem", *render, "chem");
        }
        return;
    }
    if (panelId == kActiveStructurePanel) {
        g.AddNode("view.structure_list", 380, 60);
        for (int i = 0; i < (int)state.structures.size(); ++i) AddLoadNode(state, g, i);
        return;
    }
    if (panelId == kPlotPanel) {
        g.AddNode("view.plot_view", 40, 60);
        return;
    }
    // Not decomposed yet: the whole panel as one node, when the UI registered one.
    g.AddNode("panel." + panelId, 40, 60);
}

void OnStructureLoaded(AppState& state, int index) {
    GraphSystem& gs = state.GraphSys();
    if (!gs.HasPanel(kActiveStructurePanel)) return;   // seeding later picks it up
    if (index < 0 || index >= (int)state.structures.size()) return;
    Graph& g = gs.panelGraphs[kActiveStructurePanel].graph;
    const std::string& path = state.structures[(size_t)index].path;
    if (path.empty() || FindLoadNode(g, AbsolutePath(path))) return;
    AddLoadNode(state, g, index);
}

void OnStructureRemoved(AppState& state, const std::string& path) {
    GraphSystem& gs = state.GraphSys();
    if (!gs.HasPanel(kActiveStructurePanel) || path.empty()) return;
    Graph& g = gs.panelGraphs[kActiveStructurePanel].graph;
    while (Node* n = FindLoadNode(g, AbsolutePath(path))) g.RemoveNode(n->id);
}

const Atoms* ViewAtoms(AppState& state) {
    const View3DRequest& req = state.GraphSys().view3d;
    return req.valid ? &req.atoms : nullptr;
}

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
                "Draws the incoming ChemicalData in the Structure View (style, grid, ...).",
                {{"chem", ValueType::Chem}},
                {},
                &EvalRender3D, &BodyRender3D});
    r.Register({"view.load_structure", "Load Structure", NodeKind::Build, "View",
                "Reads an xyz file into the structure list.",
                {},
                {{"structure", ValueType::Structure}},
                &EvalLoadStructure, &BodyLoadStructure});
    std::vector<PinSpec> listPins;
    for (int k = 0; k < kStructureListPins; ++k) listPins.push_back({fmt::format("s{}", k + 1), ValueType::Structure});
    r.Register({"view.structure_list", "Structure List", NodeKind::Other, "View",
                "The loaded structures; pick the active one. Feeds the Active Structure panel.",
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
