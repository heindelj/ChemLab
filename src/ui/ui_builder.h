#pragma once
// The interactive UI builder: pick or create a UI (name + layout), then
// drag panels from a floating palette onto the empty slots. Also the code
// that applies a UIDefinition to the ImGui dockspace.

#include "imgui.h"

struct AppState;
struct UIDefinition;

// Rebuild the dockspace node tree for `def` and dock its panels into it.
// Must run before ImGui::DockSpace() in the frame.
void ApplyUIDockLayout(AppState& state, const UIDefinition& def, ImGuiID dockspaceId, ImVec2 size);

// Show/hide panel windows according to `def` (panels not in the UI hide).
void ApplyUIVisibility(AppState& state, const UIDefinition& def);

// Per-frame hook, called just before ImGui::DockSpace(): rebuilds the dock
// tree with slot placeholders while the builder's edit mode needs it.
void UIBuilderPreDockspace(AppState& state, ImGuiID dockspaceId, ImVec2 size);

// The builder windows (call after ImGui::DockSpace()):
//  - the "UI Builder" window (choose / create / manage UIs),
//  - in edit mode: one placeholder window per slot plus the floating
//    "Panels" palette that panels are dragged out of.
void DrawUIBuilder(AppState& state);

// User-defined UIs are stored in chemlab_uis.toml (working directory).
void LoadUserUIsIntoState(AppState& state);
void SaveUserUIsFromState(AppState& state);
