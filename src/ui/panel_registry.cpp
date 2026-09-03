#include "ui/panel_registry.h"

#include <string>

#include "imgui.h"

#include "graph/node_registry.h"
#include "ui/ui.h"

const std::vector<PanelInfo>& PanelCatalog() {
    static const std::vector<PanelInfo> catalog = {
        {"structure_view", PanelName::StructureView, "3D view of the active structure.", &DrawStructureViewPanel, true},
        {"plot_2d", PanelName::Plot, "2D plot of energy or measurements against frame.", &DrawPlotPanel},
        {"controls", PanelName::Controls, "Structure list, playback, and render settings.", &DrawControlsPanel},
        {"active_structure", PanelName::ActiveStructure, "Details of the active file: atoms, frames, selection.",
         &DrawActiveStructurePanel},
        {"calculate", PanelName::Calculate, "Run calculations on the active structure.", &DrawCalculatePanel},
        {"output", PanelName::Output, "Output from calculations.", &DrawOutputPanel},
        {"export", PanelName::Export, "Screenshots and xyz export.", &DrawExportPanel},
        {"console", PanelName::Console, "Command log and console.", &DrawConsolePanel},
        {"node_graph", PanelName::NodeGraph, "Node graph: chain data sources, scripts and analyses.", &DrawNodeGraphPanel},
        {"graph_canvas", PanelName::GraphCanvas,
         "Graph canvas: a blank graph to sketch build -> simulate -> analyze -> visualize pipelines in; saved by name.",
         &DrawGraphCanvasPanel},
    };
    return catalog;
}

const PanelInfo* FindPanel(const std::string& id) {
    for (const auto& p : PanelCatalog())
        if (id == p.id) return &p;
    return nullptr;
}

void RegisterPanelNodes() {
    for (const PanelInfo& p : PanelCatalog()) {
        if (IsFreeGraphPanel(p.id)) continue;   // the free-form graphs are not panel graphs
        graph::NodeTypeSpec spec;
        spec.id = std::string("panel.") + p.id;
        spec.name = p.title;
        spec.kind = graph::NodeKind::Visualize;
        spec.category = "Panels";
        spec.description = std::string(p.description) + " (the whole panel as one node)";
        spec.evaluate = [](AppState&, graph::Node&, const std::vector<const graph::Value*>&, std::vector<graph::Value>&) {
            return std::string{};
        };
        const std::string note = std::string("built-in ") + p.title + " panel;\nnot decomposed into nodes yet";
        spec.drawBody = [note](AppState&, graph::Node&) {
            ImGui::TextDisabled("%s", note.c_str());
            return false;
        };
        graph::NodeTypes().Register(std::move(spec));
    }
}
