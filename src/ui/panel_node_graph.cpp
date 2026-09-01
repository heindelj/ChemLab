// Node Graph panel: draws graph::Graph with imgui-node-editor and lets the
// user wire data sources, scripts and analyses together. All graph/data logic
// lives in src/graph -- this file is only the canvas.

#include <string>

#include <fmt/format.h>

#include "imgui.h"
#include "imgui_node_editor.h"

#include "app/app_state.h"
#include "graph/graph_system.h"
#include "graph/py_runner.h"
#include "ui/ui.h"

namespace ed = ax::NodeEditor;

namespace {

ed::EditorContext* gEditor = nullptr;

ed::EditorContext* Editor() {
    if (!gEditor) {
        ed::Config config;
        config.SettingsFile = "chemlab_nodes.json";   // node positions etc.
        gEditor = ed::CreateEditor(&config);
    }
    return gEditor;
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
        default: return {0.75f, 0.75f, 0.75f, 1.0f};
    }
}

void DrawNode(AppState& state, graph::Node& node) {
    const graph::NodeTypeSpec* spec = graph::NodeTypes().Find(node.typeId);
    if (node.posDirty) {
        ed::SetNodePosition(node.id, ImVec2(node.posX, node.posY));
        node.posDirty = false;
    }
    ed::BeginNode(node.id);
    ImGui::PushID((int)node.id);
    ImGui::TextUnformatted(node.title.c_str());
    ImGui::Spacing();

    ImGui::BeginGroup();   // inputs
    for (size_t i = 0; i < node.inputs.size(); ++i) {
        ed::BeginPin(graph::InPinId(node.id, (int)i), ed::PinKind::Input);
        ImGui::TextColored(TypeColor(node.inputs[i].type), "-> %s", node.inputs[i].name.c_str());
        ed::EndPin();
    }
    if (node.inputs.empty()) ImGui::Dummy(ImVec2(1, 1));
    ImGui::EndGroup();
    ImGui::SameLine(0.0f, 24.0f);
    ImGui::BeginGroup();   // outputs
    for (size_t i = 0; i < node.outputs.size(); ++i) {
        ed::BeginPin(graph::OutPinId(node.id, (int)i), ed::PinKind::Output);
        ImGui::TextColored(TypeColor(node.outputs[i].type), "%s ->", node.outputs[i].name.c_str());
        ed::EndPin();
    }
    if (node.outputs.empty()) ImGui::Dummy(ImVec2(1, 1));
    ImGui::EndGroup();

    if (spec && spec->drawBody) {
        ImGui::Spacing();
        spec->drawBody(state, node);
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
}

void HandleLinkCreation(graph::Graph& g) {
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
    }
    ed::EndCreate();
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
        std::string lastCategory;
        for (const auto& t : graph::NodeTypes().All()) {
            if (t.category != lastCategory) {
                if (!lastCategory.empty()) ImGui::Separator();
                ImGui::TextDisabled("%s", t.category.c_str());
                lastCategory = t.category;
            }
            if (ImGui::MenuItem(t.name.c_str())) {
                graph::Node* n = g.AddNode(t.id, spawnPos.x, spawnPos.y);
                (void)n;
            }
            if (ImGui::IsItemHovered() && ImGui::BeginTooltip()) {
                ImGui::TextUnformatted(t.description.c_str());
                ImGui::EndTooltip();
            }
        }
        ImGui::EndPopup();
    }
}

}  // namespace

void DrawNodeGraphPanel(AppState& state) {
    graph::GraphSystem& gs = state.GraphSys();

    // ---- toolbar ----
    if (ImGui::Button("Run graph")) gs.Run(state);
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
    ed::SetCurrentEditor(Editor());
    ed::Begin("chemlab_node_graph", ImVec2(0, 0));
    const ImVec2 mouseCanvas = ed::ScreenToCanvas(ImGui::GetMousePos());

    for (auto& node : gs.graph.nodes) DrawNode(state, node);
    for (const auto& link : gs.graph.links)
        ed::Link(link.id, graph::OutPinId(link.fromNode, link.fromPin), graph::InPinId(link.toNode, link.toPin));

    HandleLinkCreation(gs.graph);
    HandleDeletion(gs.graph);

    ed::Suspend();
    AddNodePopup(gs.graph, mouseCanvas);
    ed::Resume();

    ed::End();
    ed::SetCurrentEditor(nullptr);
}

void NodeGraphShutdown() {
    if (gEditor) {
        ed::DestroyEditor(gEditor);
        gEditor = nullptr;
    }
}
