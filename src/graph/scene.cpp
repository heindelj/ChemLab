#include "graph/scene.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <set>

#include <fmt/format.h>
#include <toml++/toml.hpp>

#include "imgui.h"
#include "imgui_stdlib.h"

#include "app/app_state.h"
#include "graph/graph_io.h"
#include "graph/graph_system.h"
#include "ui/panel_registry.h"
#include "ui/ui_builder.h"

namespace graph {

namespace {

constexpr const char* kActiveFile = "chemlab_scene.toml";

std::string TextParam(const Node& n, const std::string& key, const std::string& fallback = "") {
    auto it = n.params.find(key);
    if (it == n.params.end()) return fallback;
    const std::string* s = it->second.AsText();
    return s ? *s : fallback;
}

int64_t IntParam(const Node& n, const std::string& key, int64_t fallback) {
    auto it = n.params.find(key);
    int64_t v = fallback;
    if (it == n.params.end() || !it->second.AsInt(v)) return fallback;
    return v;
}

bool IsPanelNode(const Node& n) { return n.typeId.rfind(kPanelNodePrefix, 0) == 0; }
std::string PanelIdOf(const Node& n) { return n.typeId.substr(std::string(kPanelNodePrefix).size()); }

// Which scene (index) owns this node, -1 when none.
int OwningSceneIndex(const GraphSystem& gs, const Node& n) {
    for (size_t i = 0; i < gs.scenes.size(); ++i)
        for (const Node& m : gs.scenes[i].graph.nodes)
            if (&m == &n) return (int)i;
    return -1;
}

// The panels a node stands for, read from the wiring: a Panel node is one
// panel, a Tabs node is its inputs in order.
void CollectPanels(const Graph& g, const Node& n, std::vector<UIPanelRef>& out, int depth = 0) {
    if (depth > 8) return;
    if (IsPanelNode(n)) {
        out.push_back({PanelIdOf(n), IntParam(n, "visible", 1) != 0});
        return;
    }
    if (n.typeId == kTabsNodeType) {
        for (int k = 0; k < (int)n.inputs.size(); ++k)
            if (const Link* l = g.LinkInto(n.id, k))
                if (const Node* up = g.FindNode(l->fromNode)) CollectPanels(g, *up, out, depth + 1);
    }
}

bool HasOutgoingLink(const Graph& g, const Node& n) {
    for (const Link& l : g.links)
        if (l.fromNode == n.id) return true;
    return false;
}

Node* FindPanelNode(Graph& g, const std::string& panelId) {
    for (Node& n : g.nodes)
        if (IsPanelNode(n) && PanelIdOf(n) == panelId) return &n;
    return nullptr;
}

// A Layout node saved by the first scene build kept its panels as "slotN"
// text params ("calculate, output, ~console") instead of wiring. Turn those
// into Panel/Tabs nodes plugged into the slots, once, on load.
void MigrateSlotParams(Graph& g, Node& layout) {
    bool legacy = false;
    for (const auto& [key, v] : layout.params) legacy |= key.rfind("slot", 0) == 0;
    if (!legacy || LayoutHasPanels(g, layout)) return;
    UIDefinition ui = LayoutUI(g, layout);
    for (size_t k = 0; k < ui.slots.size(); ++k) {
        const std::string text = TextParam(layout, fmt::format("slot{}", k));
        size_t start = 0;
        while (start <= text.size()) {
            size_t end = text.find(',', start);
            if (end == std::string::npos) end = text.size();
            std::string id = text.substr(start, end - start);
            while (!id.empty() && id.front() == ' ') id.erase(id.begin());
            while (!id.empty() && id.back() == ' ') id.pop_back();
            if (!id.empty()) {
                UIPanelRef ref;
                if (id[0] == '~') { ref.visible = false; id.erase(0, 1); }
                ref.panel = id;
                ui.slots[k].push_back(ref);
            }
            start = end + 1;
        }
    }
    for (auto it = layout.params.begin(); it != layout.params.end();)
        it = it->first.rfind("slot", 0) == 0 ? layout.params.erase(it) : std::next(it);
    // The old node sat where the Panel nodes go now: move it to the right.
    layout.posX = 560.0f;
    layout.posY = 60.0f;
    layout.posDirty = true;
    SetLayoutUI(g, &layout, ui);
}

// ---- node evaluation / bodies ----------------------------------------------

std::string EvalLayout(AppState&, Node& n, const std::vector<const Value*>& in, std::vector<Value>&) {
    for (size_t k = 0; k < in.size(); ++k)
        if (in[k] && !in[k]->AsPanels()) return fmt::format("wrong input type on '{}'", n.inputs[k].name);
    return "";
}

std::string EvalTabs(AppState&, Node& n, const std::vector<const Value*>& in, std::vector<Value>& out) {
    PanelList all;
    for (size_t k = 0; k < in.size(); ++k) {
        if (!in[k]) continue;
        const PanelList* p = in[k]->AsPanels();
        if (!p) return fmt::format("wrong input type on '{}'", n.inputs[k].name);
        all.insert(all.end(), p->begin(), p->end());
    }
    out[0].v = std::move(all);
    return "";
}

// The node body: the layout's name, its kind (cycled with < >, since node
// bodies cannot open combos; locked while panels are wired in) and, when
// it is not the layout on screen, a Show button.
bool BodyLayout(AppState& s, Node& n) {
    GraphSystem& gs = s.GraphSys();
    const int sceneIndex = OwningSceneIndex(gs, n);
    Graph* g = OwningGraph(gs, n);
    bool changed = false;

    std::string name = TextParam(n, "name");
    ImGui::PushItemWidth(160.0f);
    if (ImGui::InputText("name", &name)) {
        n.params["name"] = Value::S(name);
        n.title = name.empty() ? "Layout" : name;
        changed = true;
    }
    ImGui::PopItemWidth();

    const auto& layouts = BuiltinLayouts();
    const std::string layoutId = TextParam(n, "layout", "single");
    int index = 0;
    for (int i = 0; i < (int)layouts.size(); ++i)
        if (layouts[i].id == layoutId) index = i;
    const bool locked = g && LayoutHasPanels(*g, n);
    auto setLayout = [&](int i) {
        i = ((i % (int)layouts.size()) + (int)layouts.size()) % (int)layouts.size();
        n.params["layout"] = Value::S(layouts[i].id);
        if (g) SyncLayoutPins(*g, n);
        changed = true;
    };
    ImGui::BeginDisabled(locked);
    if (ImGui::SmallButton("<")) setLayout(index - 1);
    ImGui::SameLine();
    ImGui::Text("%s", layouts[index].name.c_str());
    ImGui::SameLine();
    if (ImGui::SmallButton(">")) setLayout(index + 1);
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::TextDisabled(locked ? "(locked: slots are wired)" : "(%d slot%s)", layouts[index].SlotCount(),
                        layouts[index].SlotCount() == 1 ? "" : "s");
    if (locked && ImGui::IsItemHovered()) ImGui::SetTooltip("Unplug every slot to change the layout.");

    if (sceneIndex >= 0) {
        Scene& scene = gs.scenes[(size_t)sceneIndex];
        const bool shown = sceneIndex == gs.activeScene && ActiveLayoutNode(scene) == &n;
        if (shown) {
            ImGui::TextDisabled("on screen");
        } else if (ImGui::SmallButton("Show")) {
            scene.activeLayout = n.id;
            gs.SwitchScene(s, sceneIndex);
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("UI Builder")) UIBuilderEditLayout(s, sceneIndex, n.id);
    }
    return changed;
}

bool BodyTabs(AppState&, Node&) {
    ImGui::TextDisabled("panels become tabs, top to bottom");
    return false;
}

std::string EvalPanel(AppState&, Node& n, const std::vector<const Value*>&, std::vector<Value>& out) {
    out[0].v = PanelList{{PanelIdOf(n), IntParam(n, "visible", 1) != 0}};
    return "";
}

bool BodyPanel(AppState&, Node& n) {
    bool visible = IntParam(n, "visible", 1) != 0;
    if (ImGui::Checkbox("shown at startup", &visible)) {
        n.params["visible"] = Value::I(visible ? 1 : 0);
        return true;
    }
    return false;
}

}  // namespace

// ---------------------------------------------------------------------------
// Layout nodes
// ---------------------------------------------------------------------------
std::vector<Node*> LayoutNodes(Graph& g) {
    std::vector<Node*> out;
    for (Node& n : g.nodes)
        if (n.typeId == kLayoutNodeType) out.push_back(&n);
    return out;
}

Node* LayoutNode(Graph& g) {
    for (Node& n : g.nodes)
        if (n.typeId == kLayoutNodeType) return &n;
    return nullptr;
}

const Node* LayoutNode(const Graph& g) { return LayoutNode(const_cast<Graph&>(g)); }

Node* ActiveLayoutNode(Scene& s) {
    if (s.activeLayout != 0)
        if (Node* n = s.graph.FindNode(s.activeLayout); n && n->typeId == kLayoutNodeType) return n;
    return LayoutNode(s.graph);
}

const Node* ActiveLayoutNode(const Scene& s) { return ActiveLayoutNode(const_cast<Scene&>(s)); }

std::string LayoutName(const Node& layout) {
    const std::string name = TextParam(layout, "name");
    return name.empty() ? layout.title : name;
}

Node* FindLayoutNode(Graph& g, const std::string& name) {
    for (Node* n : LayoutNodes(g))
        if (LayoutName(*n) == name) return n;
    return nullptr;
}

void SyncLayoutPins(Graph& g, Node& layout) {
    const LayoutDef* def = FindLayout(TextParam(layout, "layout", "single"));
    const int slots = def ? def->SlotCount() : 1;
    if ((int)layout.inputs.size() == slots) return;
    layout.inputs.clear();
    for (int k = 0; k < slots; ++k) layout.inputs.push_back({fmt::format("slot{}", k + 1), ValueType::Panel});
    layout.outputs.clear();
    layout.outValues.clear();
    g.PruneLinks();
}

bool LayoutHasPanels(const Graph& g, const Node& layout) {
    for (int k = 0; k < (int)layout.inputs.size(); ++k)
        if (g.LinkInto(layout.id, k)) return true;
    return false;
}

std::string SceneName(const Scene& s) {
    if (const Node* n = LayoutNode(s.graph)) return LayoutName(*n);
    return s.graphName.empty() ? "untitled" : s.graphName;
}

UIDefinition LayoutUI(const Graph& g, const Node& layout) {
    UIDefinition ui;
    ui.name = LayoutName(layout);
    ui.layoutId = TextParam(layout, "layout", "single");
    const LayoutDef* def = FindLayout(ui.layoutId);
    ui.slots.resize(def ? def->SlotCount() : 1);
    for (int k = 0; k < (int)layout.inputs.size() && k < (int)ui.slots.size(); ++k)
        if (const Link* l = g.LinkInto(layout.id, k))
            if (const Node* up = g.FindNode(l->fromNode)) CollectPanels(g, *up, ui.slots[(size_t)k]);
    return ui;
}

UIDefinition SceneUI(const Scene& s) {
    UIDefinition ui;
    if (const Node* n = ActiveLayoutNode(s)) ui = LayoutUI(s.graph, *n);
    else { ui.layoutId = "single"; ui.slots.resize(1); }
    if (ui.name.empty()) ui.name = SceneName(s);
    ui.builtin = s.builtin;
    return ui;
}

uint64_t LayoutStamp(const Scene& s) {
    uint64_t h = 1469598103934665603ull;
    auto mix = [&](uint64_t v) { h = (h ^ v) * 1099511628211ull; };
    auto mixText = [&](const std::string& t) {
        for (char c : t) mix((uint64_t)(unsigned char)c);
        mix(0x0a);
    };
    const Node* layout = ActiveLayoutNode(s);
    if (!layout) return h;
    mix(layout->id);
    const UIDefinition ui = LayoutUI(s.graph, *layout);
    mixText(ui.layoutId);
    for (const auto& slot : ui.slots) {
        for (const UIPanelRef& r : slot) {
            mixText(r.panel);
            mix(r.visible);
        }
        mix(0x3a);
    }
    return h;
}

Node& SetLayoutUI(Graph& g, Node* layout, const UIDefinition& ui) {
    if (!layout) {
        const int existing = (int)LayoutNodes(g).size();
        layout = g.AddNode(kLayoutNodeType, 560.0f + 440.0f * (float)existing, 60.0f);
    }
    Node& L = *layout;
    L.params["name"] = Value::S(ui.name);
    L.title = ui.name.empty() ? "Layout" : ui.name;
    // Unplug every slot first (so the layout may change), then rewire.
    g.links.erase(std::remove_if(g.links.begin(), g.links.end(), [&](const Link& l) { return l.toNode == L.id; }),
                  g.links.end());
    L.params["layout"] = Value::S(ui.layoutId);
    L.inputs.clear();
    SyncLayoutPins(g, L);

    int placed = 0;   // new Panel nodes go in a column on the left
    auto panelNode = [&](const UIPanelRef& ref) -> Node* {
        Node* p = FindPanelNode(g, ref.panel);
        if (!p) {
            int panels = 0;
            for (const Node& n : g.nodes) panels += IsPanelNode(n);
            p = g.AddNode(kPanelNodePrefix + ref.panel, 40.0f, 60.0f + 105.0f * (float)panels);
            if (p)
                if (const PanelInfo* info = FindPanel(ref.panel)) p->title = info->title;
            ++placed;
        }
        if (p) p->params["visible"] = Value::I(ref.visible ? 1 : 0);
        return p;
    };
    for (size_t s = 0; s < ui.slots.size() && s < L.inputs.size(); ++s) {
        const auto& refs = ui.slots[s];
        if (refs.empty()) continue;
        if (refs.size() == 1) {
            if (Node* p = panelNode(refs[0])) g.AddLink(p->id, 0, L.id, (int)s);
            continue;
        }
        Node* tabs = g.AddNode(kTabsNodeType, L.posX - 300.0f, L.posY + 40.0f + 150.0f * (float)s);
        if (!tabs) continue;
        for (size_t k = 0; k < refs.size() && k < (size_t)kTabsPins; ++k)
            if (Node* p = panelNode(refs[k])) g.AddLink(p->id, 0, tabs->id, (int)k);
        g.AddLink(tabs->id, 0, L.id, (int)s);
    }
    // Panel/Tabs nodes that feed nothing any more are gone. (Removing a node
    // from the deque invalidates references: re-find the layout afterwards.)
    const uint32_t layoutId = L.id;
    for (bool again = true; again;) {
        again = false;
        for (const Node& n : g.nodes)
            if ((IsPanelNode(n) || n.typeId == kTabsNodeType) && !HasOutgoingLink(g, n)) {
                g.RemoveNode(n.id);
                again = true;
                break;
            }
    }
    (void)placed;
    g.Touch();
    return *g.FindNode(layoutId);
}

void SetSceneUI(Scene& s, const UIDefinition& ui) {
    Node& L = SetLayoutUI(s.graph, ActiveLayoutNode(s), ui);
    s.activeLayout = L.id;
}

Scene MakeScene(const UIDefinition& ui, bool builtin) {
    Scene s;
    s.builtin = builtin;
    s.graphName = ui.name;
    SetSceneUI(s, ui);
    return s;
}

std::vector<Scene> BuiltinScenes() {
    std::vector<Scene> scenes;
    for (const UIDefinition& ui : BuiltinUIs()) scenes.push_back(MakeScene(ui, true));
    return scenes;
}

// ---------------------------------------------------------------------------
// Files
// ---------------------------------------------------------------------------
std::string ScenesDir() { return "scenes"; }
std::string ScenePath(const std::string& graphName) { return ScenesDir() + "/" + graphName + ".json"; }

bool SaveScene(Scene& s, std::string& err) {
    if (!IsScene(s.graph)) { err = "the graph has no Layout node, so it is not a scene"; return false; }
    if (s.graphName.empty()) s.graphName = SceneName(s);
    std::error_code ec;
    std::filesystem::create_directories(ScenesDir(), ec);
    return SaveGraph(s.graph, ScenePath(s.graphName), err);
}

bool LoadSceneFile(const std::string& path, Scene& out, std::string& err) {
    Scene s;
    if (!LoadGraph(path, s.graph, err)) return false;
    if (!IsScene(s.graph)) { err = fmt::format("{} has no Layout node, so it is not a scene", path); return false; }
    for (Node* n : LayoutNodes(s.graph)) {
        SyncLayoutPins(s.graph, *n);
        MigrateSlotParams(s.graph, *n);
    }
    s.graphName = std::filesystem::path(path).stem().string();
    out = std::move(s);
    return true;
}

std::vector<Scene> LoadUserScenes(std::vector<std::string>& errors) {
    std::vector<Scene> scenes;
    std::vector<std::filesystem::path> files;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(ScenesDir(), ec))
        if (entry.is_regular_file() && entry.path().extension() == ".json") files.push_back(entry.path());
    std::sort(files.begin(), files.end());
    for (const auto& f : files) {
        Scene s;
        std::string err;
        if (LoadSceneFile(f.string(), s, err)) scenes.push_back(std::move(s));
        else errors.push_back(err);
    }
    return scenes;
}

std::string ActiveSceneFile() { return kActiveFile; }

void ReadActiveScene(std::string& scene, std::string& layout) {
    scene.clear();
    layout.clear();
    try {
        if (!std::filesystem::exists(kActiveFile)) return;
        const toml::table t = toml::parse_file(kActiveFile);
        scene = t["active"].value_or(std::string{});
        layout = t["layout"].value_or(std::string{});
    } catch (...) {
    }
}

void WriteActiveScene(const std::string& scene, const std::string& layout) {
    std::ofstream f(kActiveFile, std::ios::trunc);
    if (f)
        f << "# ChemLab: the scene and layout shown at startup (see `scene list`).\nactive = \"" << scene
          << "\"\nlayout = \"" << layout << "\"\n";
}

// ---------------------------------------------------------------------------
// Node types
// ---------------------------------------------------------------------------
void RegisterSceneNodes(NodeTypeRegistry& r) {
    r.Register({kLayoutNodeType, "Layout", NodeKind::Visualize, "Scene",
                "Arranges panels in the slots of a layout; a graph with a Layout node is a scene. "
                "Wire Panel (or Tabs) nodes into the slots.",
                {{"slot1", ValueType::Panel}},
                {},
                &EvalLayout, &BodyLayout});
    std::vector<PinSpec> tabPins;
    for (int k = 0; k < kTabsPins; ++k) tabPins.push_back({fmt::format("t{}", k + 1), ValueType::Panel});
    r.Register({kTabsNodeType, "Tabs", NodeKind::Visualize, "Scene",
                "Several panels in one layout slot, as tabs (top input first).",
                tabPins,
                {{"panel", ValueType::Panel}},
                &EvalTabs, &BodyTabs});
    for (const PanelInfo& p : PanelCatalog()) {
        NodeTypeSpec spec;
        spec.id = kPanelNodePrefix + std::string(p.id);
        spec.name = p.title;
        spec.kind = NodeKind::Visualize;
        spec.category = "Panels";
        spec.description = std::string(p.description) + " Plug into a Layout slot (or a Tabs node).";
        spec.outputs = {{"panel", ValueType::Panel}};
        spec.evaluate = &EvalPanel;
        spec.drawBody = &BodyPanel;
        r.Register(std::move(spec));
    }
}

}  // namespace graph
