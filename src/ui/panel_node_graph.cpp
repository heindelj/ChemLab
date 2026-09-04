// Node Graph panel: draws graph::Graph with imgui-node-editor and lets the
// user wire data sources, scripts and analyses together. All graph/data logic
// lives in src/graph -- this file is only the canvas. The same canvas draws
// the Graph Canvas panel (DrawGraphCanvasPanel) and the scene graphs in their
// "Scene graph: <name>" windows (DrawSceneGraphWindows), one editor context per
// graph. Nodes are coloured by their kind (graph::NodeKind): build orange,
// simulate purple, analyze green, visualize cyan, other grey.

#include <algorithm>
#include <cctype>
#include <map>
#include <set>
#include <string>
#include <vector>

#include <fmt/format.h>

#include "imgui.h"
#include "imgui_internal.h"   // SetFontRasterizerDensity
#include "imgui_stdlib.h"
#include "imgui_node_editor.h"

#include "app/app_state.h"
#include "graph/graph_system.h"
#include "graph/py_runner.h"
#include "ui/panel_registry.h"
#include "ui/theme.h"
#include "ui/ui.h"

namespace ed = ax::NodeEditor;

namespace {

// One editor context per graph: node ids restart at 1 in every graph, and
// each context keeps its own view/selection. Only the free-form graph
// persists its node positions to disk; canvas and scene graphs keep theirs
// in the graph itself (Node::posX/posY, saved with the graph).
std::map<std::string, ed::EditorContext*> gEditors;

ed::EditorContext* Editor(const std::string& key, const char* settingsFile) {
    auto it = gEditors.find(key);
    if (it != gEditors.end()) return it->second;
    ed::Config config;
    config.SettingsFile = settingsFile ? settingsFile : "";   // "" = no persistence
    ed::EditorContext* ctx = ed::CreateEditor(&config);
    gEditors[key] = ctx;
    return ctx;
}

ImVec4 TypeColor(graph::ValueType t) {
    using VT = graph::ValueType;
    switch (t) {
        case VT::Float: return {0.55f, 0.85f, 1.0f, 1.0f};
        case VT::Int: return {0.6f, 1.0f, 0.6f, 1.0f};
        case VT::Text: return {1.0f, 0.85f, 0.55f, 1.0f};
        case VT::FloatVec: return {0.85f, 0.7f, 1.0f, 1.0f};
        case VT::Positions: return {1.0f, 0.6f, 0.6f, 1.0f};
        case VT::Labels: return {1.0f, 1.0f, 0.6f, 1.0f};
        case VT::Table: return {0.6f, 0.9f, 0.85f, 1.0f};
        case VT::Series: return {1.0f, 0.7f, 0.85f, 1.0f};
        case VT::Chem: return {0.75f, 0.95f, 0.65f, 1.0f};
        case VT::Structure: return {0.95f, 0.8f, 0.6f, 1.0f};
        case VT::Panel: return {0.5f, 0.9f, 0.95f, 1.0f};
        default: return {0.75f, 0.75f, 0.75f, 1.0f};
    }
}

// Header tint: the node's kind (build / simulate / analyze / visualize / other).
ImVec4 KindTint(const graph::NodeTypeSpec* spec) {
    const graph::KindColor c = graph::ColorOf(spec ? spec->kind : graph::NodeKind::Other);
    return {c.r, c.g, c.b, c.a};
}

// The kind colour itself (menu headers, legend), optionally with another alpha.
ImVec4 KindColor(graph::NodeKind k, float alpha = 1.0f) {
    const graph::KindColor c = graph::ColorOf(k);
    return {c.r, c.g, c.b, alpha};
}

ImVec4 Mix(const ImVec4& a, const ImVec4& b, float t) {
    return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t};
}

// Editor chrome derived from the active theme, pushed around every canvas.
struct EditorStyleScope {
    int colors = 0, vars = 0;
    explicit EditorStyleScope(UITheme theme) {
        const UIPalette p = ThemePalette(theme);
        auto col = [&](ed::StyleColor idx, const ImVec4& c) { ed::PushStyleColor(idx, c); ++colors; };
        auto var = [&](ed::StyleVar idx, auto v) { ed::PushStyleVar(idx, v); ++vars; };
        col(ed::StyleColor_Bg, p.bg);
        col(ed::StyleColor_Grid, ImVec4(p.border.x, p.border.y, p.border.z, 0.5f));
        col(ed::StyleColor_NodeBg, ImVec4(p.bgPanel.x, p.bgPanel.y, p.bgPanel.z, 0.97f));
        col(ed::StyleColor_NodeBorder, ImVec4(p.border.x, p.border.y, p.border.z, 1.0f));
        col(ed::StyleColor_HovNodeBorder, p.accentHover);
        col(ed::StyleColor_SelNodeBorder, p.accent);
        col(ed::StyleColor_NodeSelRect, ImVec4(p.accent.x, p.accent.y, p.accent.z, 0.15f));
        col(ed::StyleColor_NodeSelRectBorder, ImVec4(p.accent.x, p.accent.y, p.accent.z, 0.6f));
        col(ed::StyleColor_HovLinkBorder, p.accentHover);
        col(ed::StyleColor_SelLinkBorder, p.accent);
        col(ed::StyleColor_HighlightLinkBorder, p.accentActive);
        col(ed::StyleColor_LinkSelRect, ImVec4(p.accent.x, p.accent.y, p.accent.z, 0.15f));
        col(ed::StyleColor_LinkSelRectBorder, ImVec4(p.accent.x, p.accent.y, p.accent.z, 0.6f));
        col(ed::StyleColor_PinRect, ImVec4(p.accent.x, p.accent.y, p.accent.z, 0.25f));
        col(ed::StyleColor_PinRectBorder, ImVec4(p.accent.x, p.accent.y, p.accent.z, 0.5f));
        col(ed::StyleColor_Flow, p.accentHover);
        col(ed::StyleColor_FlowMarker, p.accentHover);
        var(ed::StyleVar_NodePadding, ImVec4(10, 6, 10, 8));
        var(ed::StyleVar_NodeRounding, 6.0f);
        var(ed::StyleVar_NodeBorderWidth, 1.0f);
        var(ed::StyleVar_HoveredNodeBorderWidth, 2.0f);
        var(ed::StyleVar_SelectedNodeBorderWidth, 2.0f);
        var(ed::StyleVar_HoveredNodeBorderOffset, 0.0f);
        var(ed::StyleVar_SelectedNodeBorderOffset, 0.0f);
        var(ed::StyleVar_PinRounding, 4.0f);
        var(ed::StyleVar_PinBorderWidth, 0.0f);
        var(ed::StyleVar_LinkStrength, 120.0f);
        var(ed::StyleVar_GridSize, ImVec2(24.0f, 24.0f));   // ImVec2 in this fork, not a float
    }
    ~EditorStyleScope() {
        ed::PopStyleVar(vars);
        ed::PopStyleColor(colors);
    }
};

// Text inside the canvas is laid out at the base font size and its vertices
// are scaled by the view; without this the glyph bitmaps are stretched too
// (blurry zoomed in, aliased zoomed out). ImGui 1.92 bakes glyphs per
// (size, density), so ask for the density that matches the on-screen scale.
// Zoom levels are discrete, so only a handful of bakes ever exist.
struct CanvasFontDensity {
    float base;
    CanvasFontDensity() : base(ImGui::GetFontRasterizerDensity()) { Apply(); }
    void Apply() const {
        const float zoom = ed::GetCurrentZoom();   // canvas units per screen pixel
        const float scale = zoom > 0.0f ? 1.0f / zoom : 1.0f;
        ImGui::SetFontRasterizerDensity(base * std::clamp(scale, 0.25f, 4.0f));
    }
    void Restore() const { ImGui::SetFontRasterizerDensity(base); }
    ~CanvasFontDensity() { Restore(); }
};

constexpr float kPinRadius = 5.0f;

// A pin: a dot (filled when connected) next to its name. The pivot sits on
// the dot so links end on it rather than on the text.
void DrawPin(uint64_t pinId, ed::PinKind kind, const graph::PinSpec& pin, bool connected) {
    const ImVec4 color = TypeColor(pin.type);
    const ImU32 col32 = ImGui::ColorConvertFloat4ToU32(color);
    const float lineH = ImGui::GetTextLineHeight();
    const ImVec2 dotSize(kPinRadius * 2.0f + 2.0f, lineH);

    ed::BeginPin(pinId, kind);
    ImGui::BeginGroup();
    if (kind == ed::PinKind::Input) {
        const ImVec2 dotPos = ImGui::GetCursorScreenPos();
        ImGui::Dummy(dotSize);
        ImGui::SameLine(0.0f, 5.0f);
        ImGui::TextColored(color, "%s", pin.name.c_str());
        const ImVec2 c(dotPos.x + kPinRadius + 1.0f, dotPos.y + lineH * 0.5f);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        if (connected) dl->AddCircleFilled(c, kPinRadius, col32);
        else dl->AddCircle(c, kPinRadius - 0.5f, col32, 0, 1.5f);
        ed::PinPivotRect(c, c);
    } else {
        ImGui::TextColored(color, "%s", pin.name.c_str());
        ImGui::SameLine(0.0f, 5.0f);
        const ImVec2 dotPos = ImGui::GetCursorScreenPos();
        ImGui::Dummy(dotSize);
        const ImVec2 c(dotPos.x + kPinRadius + 1.0f, dotPos.y + lineH * 0.5f);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        if (connected) dl->AddCircleFilled(c, kPinRadius, col32);
        else dl->AddCircle(c, kPinRadius - 0.5f, col32, 0, 1.5f);
        ed::PinPivotRect(c, c);
    }
    ImGui::EndGroup();
    ed::EndPin();
}

void DrawNode(AppState& state, graph::Graph& g, graph::Node& node, const std::set<uint64_t>& connectedPins) {
    const graph::NodeTypeSpec* spec = graph::NodeTypes().Find(node.typeId);
    if (node.posDirty) {
        ed::SetNodePosition(node.id, ImVec2(node.posX, node.posY));
        node.posDirty = false;
    }
    const ImVec4 headerCol = KindTint(spec);

    ed::BeginNode(node.id);
    ImGui::PushID((int)node.id);

    // ---- header: title (and the type name when it differs) ----
    ImGui::TextUnformatted(node.title.c_str());
    if (spec && spec->name != node.title) {
        ImGui::SameLine(0.0f, 8.0f);
        ImGui::TextDisabled("%s", spec->name.c_str());
    }
    const float headerBottom = ImGui::GetCursorScreenPos().y + 2.0f;   // a little breathing room
    ImGui::Dummy(ImVec2(0.0f, 4.0f));

    // ---- pins: inputs left, outputs right ----
    ImGui::BeginGroup();
    for (size_t i = 0; i < node.inputs.size(); ++i) {
        const uint64_t pid = graph::InPinId(node.id, (int)i);
        DrawPin(pid, ed::PinKind::Input, node.inputs[i], connectedPins.count(pid) > 0);
    }
    if (node.inputs.empty()) ImGui::Dummy(ImVec2(1, 1));
    ImGui::EndGroup();
    ImGui::SameLine(0.0f, 28.0f);
    ImGui::BeginGroup();
    for (size_t i = 0; i < node.outputs.size(); ++i) {
        const uint64_t pid = graph::OutPinId(node.id, (int)i);
        DrawPin(pid, ed::PinKind::Output, node.outputs[i], connectedPins.count(pid) > 0);
    }
    if (node.outputs.empty()) ImGui::Dummy(ImVec2(1, 1));
    ImGui::EndGroup();

    // ---- body ----
    if (spec && spec->drawBody) {
        ImGui::Dummy(ImVec2(0.0f, 2.0f));
        if (spec->drawBody(state, node)) g.Touch();   // a parameter changed: re-evaluate
    }
    if (!node.error.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.45f, 0.4f, 1.0f));
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 240.0f);
        ImGui::TextUnformatted(node.error.c_str());
        ImGui::PopTextWrapPos();
        ImGui::PopStyleColor();
    }
    ImGui::PopID();
    ed::EndNode();

    // ---- header band, drawn behind the content once the node's size is known ----
    const ImVec2 nodePos = ed::GetNodePosition(node.id);
    const ImVec2 nodeSize = ed::GetNodeSize(node.id);
    ImDrawList* bg = nodeSize.x > 0.0f && nodeSize.y > 0.0f ? ed::GetNodeBackgroundDrawList(node.id) : nullptr;
    if (bg) {
        const float rounding = ed::GetStyle().NodeRounding;
        const float border = ed::GetStyle().NodeBorderWidth * 0.5f;
        const ImVec2 a(nodePos.x + border, nodePos.y + border);
        const ImVec2 b(nodePos.x + nodeSize.x - border, headerBottom);
        const ImVec4 bandCol = Mix(headerCol, ThemePalette(state.theme).bgPanel, 0.35f);
        bg->AddRectFilled(a, b, ImGui::ColorConvertFloat4ToU32(ImVec4(bandCol.x, bandCol.y, bandCol.z, 1.0f)), rounding,
                          ImDrawFlags_RoundCornersTop);
        // Thin accent line under the header.
        bg->AddLine(ImVec2(a.x, b.y), ImVec2(b.x, b.y), ImGui::ColorConvertFloat4ToU32(headerCol), 1.0f);
        if (!node.error.empty()) {
            // Error badge at the top-right corner.
            const ImVec2 c(b.x - 10.0f, a.y + (b.y - a.y) * 0.5f);
            bg->AddCircleFilled(c, 4.0f, IM_COL32(255, 110, 100, 255));
        }
    }
}

void HandleLinkCreation(graph::Graph& g) {
    // EndCreate() may only follow a successful BeginCreate() (the editor
    // asserts otherwise -- silently corrupting state in a -DNDEBUG build).
    if (ed::BeginCreate()) {
        ed::PinId a, b;
        if (ed::QueryNewLink(&a, &b) && a && b) {
            uint32_t nodeA, nodeB;
            int pinA, pinB;
            bool outA, outB;
            graph::DecodePin(a.Get(), nodeA, pinA, outA);
            graph::DecodePin(b.Get(), nodeB, pinB, outB);
            if (outA == outB) {
                ed::RejectNewItem(ImVec4(1, 0.4f, 0.4f, 1), 2.0f);
            } else {
                if (!outA) {   // make A the output side
                    std::swap(nodeA, nodeB);
                    std::swap(pinA, pinB);
                }
                std::string err;
                const graph::Node* from = g.FindNode(nodeA);
                const graph::Node* to = g.FindNode(nodeB);
                const bool valid = from && to && pinA < (int)from->outputs.size() &&
                                   pinB < (int)to->inputs.size() &&
                                   graph::Compatible(from->outputs[pinA].type, to->inputs[pinB].type) &&
                                   !g.WouldCycle(nodeA, nodeB);
                if (!valid) {
                    ed::RejectNewItem(ImVec4(1, 0.4f, 0.4f, 1), 2.0f);
                } else if (ed::AcceptNewItem()) {
                    g.AddLink(nodeA, pinA, nodeB, pinB, &err);
                }
            }
        }
        ed::EndCreate();
    }
}

// Backspace (as well as the editor's own Delete) removes the selection. Only
// while the pointer is over the canvas and no text field has the keyboard,
// so backspacing in a node's text box never eats the node.
void HandleBackspace() {
    if (ImGui::GetIO().WantTextInput || !ImGui::IsKeyPressed(ImGuiKey_Backspace, false)) return;
    if (!ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem))
        return;
    std::vector<ed::NodeId> nodes((size_t)std::max(ed::GetSelectedObjectCount(), 0));
    std::vector<ed::LinkId> links(nodes.size());
    const int nn = ed::GetSelectedNodes(nodes.data(), (int)nodes.size());
    const int nl = ed::GetSelectedLinks(links.data(), (int)links.size());
    for (int i = 0; i < nn; ++i) ed::DeleteNode(nodes[(size_t)i]);
    for (int i = 0; i < nl; ++i) ed::DeleteLink(links[(size_t)i]);
    // The editor queues these; HandleDeletion picks them up through
    // BeginDelete/QueryDeleted* on this or the next frame.
}

void HandleDeletion(graph::Graph& g) {
    if (ed::BeginDelete()) {
        ed::LinkId linkId;
        while (ed::QueryDeletedLink(&linkId))
            if (ed::AcceptDeletedItem()) g.RemoveLink((uint32_t)linkId.Get());
        ed::NodeId nodeId;
        while (ed::QueryDeletedNode(&nodeId))
            if (ed::AcceptDeletedItem()) g.RemoveNode((uint32_t)nodeId.Get());
    }
    ed::EndDelete();
}

void AddNodePopup(graph::Graph& g, const ImVec2& canvasPos) {
    static ImVec2 spawnPos;
    if (ed::ShowBackgroundContextMenu()) {
        ImGui::OpenPopup("Add Node");
        spawnPos = canvasPos;
    }
    if (ImGui::BeginPopup("Add Node")) {
        // Grouped by kind (build / simulate / analyze / visualize / other),
        // each kind headed in its colour; categories inside a kind are shown
        // as a dim prefix so related nodes stay together.
        using graph::NodeKind;
        bool first = true;
        for (NodeKind kind : {NodeKind::Build, NodeKind::Simulate, NodeKind::Analyze, NodeKind::Visualize, NodeKind::Other}) {
            std::vector<const graph::NodeTypeSpec*> types;
            for (const auto& t : graph::NodeTypes().All())
                if (t.kind == kind) types.push_back(&t);
            if (types.empty()) continue;
            std::stable_sort(types.begin(), types.end(), [](const graph::NodeTypeSpec* x, const graph::NodeTypeSpec* y) {
                return x->category < y->category;
            });
            if (!first) ImGui::Separator();
            first = false;
            std::string label = graph::KindName(kind);
            label[0] = (char)toupper(label[0]);
            ImGui::TextColored(KindColor(kind), "%s", label.c_str());
            std::string lastCategory;
            for (const graph::NodeTypeSpec* t : types) {
                if (t->category != lastCategory) {
                    ImGui::TextDisabled("  %s", t->category.c_str());
                    lastCategory = t->category;
                }
                ImGui::Indent(12.0f);
                if (ImGui::MenuItem(t->name.c_str())) {
                    graph::Node* n = g.AddNode(t->id, spawnPos.x, spawnPos.y);
                    (void)n;
                }
                if (ImGui::IsItemHovered() && ImGui::BeginTooltip()) {
                    ImGui::TextUnformatted(t->description.c_str());
                    ImGui::TextDisabled("%s", t->id.c_str());
                    ImGui::EndTooltip();
                }
                ImGui::Unindent(12.0f);
            }
        }
        ImGui::EndPopup();
    }
}

}  // namespace

// The editor canvas for one graph. `key` names the editor context.
void DrawGraphEditor(AppState& state, graph::Graph& g, const std::string& key, const char* settingsFile) {
    ed::SetCurrentEditor(Editor(key, settingsFile));
    // Scoped in its own block: the style pops must run while this editor is
    // still current (popping after SetCurrentEditor(nullptr) dereferences a
    // null editor).
    {
        EditorStyleScope styleScope(state.theme);
        ed::Begin(key.c_str(), ImVec2(0, 0));
        const ImVec2 mouseCanvas = ed::ScreenToCanvas(ImGui::GetMousePos());

        std::set<uint64_t> connectedPins;
        for (const auto& link : g.links) {
            connectedPins.insert(graph::OutPinId(link.fromNode, link.fromPin));
            connectedPins.insert(graph::InPinId(link.toNode, link.toPin));
        }

        {
            CanvasFontDensity density;   // crisp glyphs at the current zoom
            for (auto& node : g.nodes) {
                DrawNode(state, g, node, connectedPins);
                // Keep the model's positions current so `graph save` sees
                // where the user actually dragged things.
                const ImVec2 p = ed::GetNodePosition(node.id);
                node.posX = p.x;
                node.posY = p.y;
            }
            for (const auto& link : g.links) {
                ImVec4 color(0.75f, 0.75f, 0.75f, 1.0f);
                if (const graph::Node* from = g.FindNode(link.fromNode))
                    if (link.fromPin >= 0 && link.fromPin < (int)from->outputs.size())
                        color = TypeColor(from->outputs[link.fromPin].type);
                color.w = 0.9f;
                ed::Link(link.id, graph::OutPinId(link.fromNode, link.fromPin), graph::InPinId(link.toNode, link.toPin), color, 2.5f);
            }

            HandleLinkCreation(g);
            HandleBackspace();
            HandleDeletion(g);

            density.Restore();   // popups are drawn in screen space
            ed::Suspend();
            AddNodePopup(g, mouseCanvas);
            ed::Resume();
            density.Apply();
        }

        ed::End();
    }
    ed::SetCurrentEditor(nullptr);
}

void DrawNodeGraphPanel(AppState& state) {
    graph::GraphSystem& gs = state.GraphSys();

    // ---- toolbar ----
    if (ImGui::Button("Run graph")) gs.Run(state);
    ImGui::SameLine();
    ImGui::Checkbox("Auto", &gs.autoRun);   // off = run on click only
    if (gs.autoRun) {
        ImGui::SameLine();
        ImGui::PushItemWidth(70.0f);
        ImGui::DragFloat("fps", &gs.autoRunFps, 0.5f, 0.1f, 120.0f, "%.1f");
        ImGui::PopItemWidth();
    }
    ImGui::SameLine();
    ImGui::TextDisabled("%zu nodes | right-click the canvas to add | `graph demo` builds an example",
                        gs.graph.nodes.size());
    if (gs.runCount > 0) {
        const ImVec4 col = gs.lastRunOk ? ImVec4(0.55f, 0.9f, 0.55f, 1) : ImVec4(1.0f, 0.45f, 0.4f, 1);
        ImGui::PushStyleColor(ImGuiCol_Text, col);
        ImGui::TextWrapped("%s", gs.lastRunSummary.c_str());
        ImGui::PopStyleColor();
    }
    ImGui::Separator();

    // ---- canvas ----
    DrawGraphEditor(state, gs.graph, "chemlab_node_graph", "chemlab_nodes.json");
}

// Graph Canvas panel: a second free-form graph for sketching build ->
// simulate -> analyze -> visualize pipelines. `graph new [name]` opens it
// blank; graphs are saved by name under graphs/<name>.json (node positions
// included -- the editor context itself keeps nothing on disk) and any saved
// graph can be loaded back from the Load dropdown.
void DrawGraphCanvasPanel(AppState& state) {
    graph::GraphSystem& gs = state.GraphSys();
    graph::CanvasGraph& cv = gs.canvas;

    // ---- toolbar: run ----
    if (ImGui::Button("Run")) gs.RunCanvas(state);
    ImGui::SameLine();
    ImGui::Checkbox("Auto", &cv.autoRun);
    if (cv.autoRun) {
        ImGui::SameLine();
        ImGui::PushItemWidth(70.0f);
        ImGui::DragFloat("fps", &gs.autoRunFps, 0.5f, 0.1f, 120.0f, "%.1f");
        ImGui::PopItemWidth();
    }
    ImGui::SameLine();
    if (ImGui::Button("New")) gs.NewCanvas(state, "untitled");
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Start a blank graph (same as `graph new`). Unsaved changes are dropped.");

    // ---- toolbar: name + save / load ----
    ImGui::SameLine(0.0f, 24.0f);
    ImGui::TextDisabled("name");
    ImGui::SameLine();
    ImGui::PushItemWidth(180.0f);
    const bool nameEntered = ImGui::InputText("##canvas_name", &cv.name, ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::PopItemWidth();
    ImGui::SameLine();
    if (ImGui::Button("Save") || nameEntered) gs.SaveCanvas(cv.name);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Save as %s", graph::GraphPath(cv.name).c_str());
    ImGui::SameLine();
    ImGui::PushItemWidth(180.0f);
    if (ImGui::BeginCombo("##canvas_load", "Load...")) {   // scanned on open, so new files show up
        const std::vector<std::string> names = graph::SavedGraphNames();
        if (names.empty()) ImGui::TextDisabled("no saved graphs in %s/", graph::GraphsDir().c_str());
        for (const std::string& n : names)
            if (ImGui::Selectable(n.c_str(), n == cv.name)) gs.LoadCanvas(state, n);
        ImGui::EndCombo();
    }
    ImGui::PopItemWidth();
    ImGui::SameLine();
    ImGui::TextDisabled("%zu nodes, %zu links | right-click the canvas to add", cv.graph.nodes.size(),
                        cv.graph.links.size());
    if (!cv.lastIoMessage.empty()) {
        ImGui::SameLine();
        ImGui::TextColored(cv.lastIoOk ? ImVec4(0.55f, 0.9f, 0.55f, 1) : ImVec4(1.0f, 0.45f, 0.4f, 1), "%s",
                           cv.lastIoMessage.c_str());
    }
    if (cv.runCount > 0) {
        const ImVec4 col = cv.lastRunOk ? ImVec4(0.55f, 0.9f, 0.55f, 1) : ImVec4(1.0f, 0.45f, 0.4f, 1);
        ImGui::PushStyleColor(ImGuiCol_Text, col);
        ImGui::TextWrapped("%s", cv.lastRunSummary.c_str());
        ImGui::PopStyleColor();
    }
    // ---- legend ----
    {
        using graph::NodeKind;
        bool first = true;
        for (NodeKind k : {NodeKind::Build, NodeKind::Simulate, NodeKind::Analyze, NodeKind::Visualize, NodeKind::Other}) {
            if (!first) ImGui::SameLine(0.0f, 14.0f);
            first = false;
            ImGui::TextColored(KindColor(k), "%s", "\xe2\x96\xa0");   // U+25A0 black square
            ImGui::SameLine(0.0f, 4.0f);
            ImGui::TextDisabled("%s", graph::KindName(k));
        }
    }
    ImGui::Separator();

    // ---- canvas ---- (no editor settings file: positions live in the saved graph)
    DrawGraphEditor(state, cv.graph, "chemlab_graph_canvas", nullptr);
}

// One "Scene graph: <name>" window per scene whose graph is open (the Graph
// button in the menu bar, View > Scene > Scene graph, or `scene <name>
// graph`). What is wired into the active Layout node is what the screen
// shows, and follows every edit; other nodes are run with the Run button.
bool DrawLayoutPicker(AppState& state, int sceneIndex, float width) {
    graph::GraphSystem& gs = state.GraphSys();
    if (sceneIndex < 0 || sceneIndex >= (int)gs.scenes.size()) return false;
    graph::Scene& sc = gs.scenes[(size_t)sceneIndex];
    const std::vector<graph::Node*> layouts = graph::LayoutNodes(sc.graph);
    if (layouts.size() <= 1) return false;
    const graph::Node* active = graph::ActiveLayoutNode(sc);
    const std::string current = active ? graph::LayoutName(*active) : "";
    ImGui::PushItemWidth(width);
    ImGui::PushID(&sc);
    if (ImGui::BeginCombo("##layout_picker", current.c_str())) {
        for (graph::Node* n : layouts)
            if (ImGui::Selectable(graph::LayoutName(*n).c_str(), n == active)) {
                sc.activeLayout = n->id;
                gs.SwitchScene(state, sceneIndex);
            }
        ImGui::EndCombo();
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Which layout of scene '%s' is on screen", graph::SceneName(sc).c_str());
    ImGui::PopID();
    ImGui::PopItemWidth();
    return true;
}

void DrawSceneGraphWindows(AppState& state) {
    graph::GraphSystem& gs = state.GraphSys();
    int cascade = 0;
    for (size_t i = 0; i < gs.scenes.size(); ++i) {
        graph::Scene& sc = gs.scenes[i];
        if (!sc.graphOpen) continue;
        const std::string name = graph::SceneName(sc);
        const std::string title = fmt::format("Scene graph: {}###scene_graph_{}", name, i);
        const ImVec2 origin = ImGui::GetMainViewport()->WorkPos;
        ImGui::SetNextWindowPos(ImVec2(origin.x + 80.0f + 40.0f * (float)cascade, origin.y + 60.0f + 40.0f * (float)cascade),
                                ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(760, 460), ImGuiCond_FirstUseEver);
        ++cascade;
        if (ImGui::Begin(title.c_str(), &sc.graphOpen, ImGuiWindowFlags_NoCollapse)) {
            const bool active = (int)i == gs.activeScene;
            if (ImGui::Button("Run")) gs.RunScene(state, sc);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Evaluate the scene graph (the Layout node itself applies as soon as it is edited).");
            ImGui::SameLine();
            if (ImGui::Button("Save")) {
                std::string err;
                sc.lastIoMessage = graph::SaveScene(sc, err) ? fmt::format("saved to {}", graph::ScenePath(sc.graphName)) : err;
                if (err.empty()) sc.builtin = false;
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Save as %s", graph::ScenePath(sc.graphName.empty() ? name : sc.graphName).c_str());
            ImGui::SameLine();
            ImGui::BeginDisabled(active);
            if (ImGui::Button("Show scene")) gs.SwitchScene(state, (int)i);
            ImGui::EndDisabled();
            ImGui::SameLine(0.0f, 16.0f);
            ImGui::TextDisabled("layout");
            ImGui::SameLine();
            if (!DrawLayoutPicker(state, (int)i, 150.0f)) {
                const graph::Node* layout = graph::ActiveLayoutNode(sc);
                ImGui::TextUnformatted(layout ? graph::LayoutName(*layout).c_str() : "none");
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("The scene's only layout; add a second Layout node to switch between them.");
            }
            ImGui::SameLine(0.0f, 16.0f);
            ImGui::TextDisabled("%s%zu nodes | graph '%s'%s | right-click the canvas to add", active ? "active | " : "",
                                sc.graph.nodes.size(), sc.graphName.c_str(), sc.builtin ? " (built-in)" : "");
            if (!sc.lastIoMessage.empty()) {
                ImGui::SameLine();
                ImGui::TextDisabled("%s", sc.lastIoMessage.c_str());
            }
            if (!graph::IsScene(sc.graph)) {
                ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.4f, 1), "This graph has no Layout node any more, so it is not a scene.");
            }
            if (sc.runCount > 0) {
                const ImVec4 col = sc.lastRunOk ? ImVec4(0.55f, 0.9f, 0.55f, 1) : ImVec4(1.0f, 0.45f, 0.4f, 1);
                ImGui::PushStyleColor(ImGuiCol_Text, col);
                ImGui::TextWrapped("%s", sc.lastRunSummary.c_str());
                ImGui::PopStyleColor();
            }
            ImGui::Separator();
            DrawGraphEditor(state, sc.graph, fmt::format("scene_graph_{}", i), nullptr);
        }
        ImGui::End();
    }
}

void NodeGraphShutdown() {
    for (auto& [key, ctx] : gEditors) ed::DestroyEditor(ctx);
    gEditors.clear();
}
