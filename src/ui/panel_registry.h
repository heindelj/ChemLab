#pragma once
// The catalog of panel types. Every dockable panel (3D view, export panel,
// active file panel, ...) is registered here with a stable id, its window
// title (which is also its dock name) and its draw function. UIDefinitions
// reference panels by id, the View menu and the UI builder's palette are
// generated from this list, and adding a panel type means adding one entry.

#include <string>
#include <vector>

struct AppState;

struct PanelInfo {
    const char* id;            // stable id used in UI definitions ("structure_view")
    const char* title;         // window title == dock window name ("Structure View")
    const char* description;   // one-liner shown in the builder palette
    void (*draw)(AppState&);   // panel body, drawn inside ImGui::Begin/End
    bool tightPadding = false; // use a small window padding (the 3D view)
};

const std::vector<PanelInfo>& PanelCatalog();
const PanelInfo* FindPanel(const std::string& id);

// Register one "panel.<id>" node type per panel (category "Panels"): the
// default graph of a panel that has not been decomposed into real nodes yet
// is that single wrapper node. Call once at startup (UIInit).
void RegisterPanelNodes();
