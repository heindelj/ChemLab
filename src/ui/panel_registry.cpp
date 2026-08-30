#include "ui/panel_registry.h"

#include "ui/ui.h"

const std::vector<PanelInfo>& PanelCatalog() {
    static const std::vector<PanelInfo> catalog = {
        {"structure_view", PanelName::StructureView, "3D view of the active structure with the 2D plot pane.",
         &DrawStructureViewPanel, true},
        {"controls", PanelName::Controls, "Structure list, playback, and render settings.", &DrawControlsPanel},
        {"active_structure", PanelName::ActiveStructure, "Details of the active file: atoms, frames, selection.",
         &DrawActiveStructurePanel},
        {"calculate", PanelName::Calculate, "Run calculations on the active structure.", &DrawCalculatePanel},
        {"output", PanelName::Output, "Output from calculations.", &DrawOutputPanel},
        {"export", PanelName::Export, "Screenshots and xyz export.", &DrawExportPanel},
        {"console", PanelName::Console, "Command log and console.", &DrawConsolePanel},
    };
    return catalog;
}

const PanelInfo* FindPanel(const std::string& id) {
    for (const auto& p : PanelCatalog())
        if (id == p.id) return &p;
    return nullptr;
}
