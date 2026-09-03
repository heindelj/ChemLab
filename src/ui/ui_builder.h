#pragma once
// The interactive UI builder: pick or create a scene (name + layout), then
// drag panels from a floating palette onto the empty slots; the result is
// written into the scene's Layout node. Also the code that applies a
// UIDefinition (what a Layout node describes) to the ImGui dockspace.

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

// Open the builder in edit mode on a Layout node of scene `sceneIndex`
// (`layoutNodeId` 0 = the scene's active layout); what is wired into the
// node becomes the draft and Save & Apply rewires it. A built-in scene is
// edited as a copy (a new scene).
void UIBuilderEditLayout(AppState& state, int sceneIndex, unsigned layoutNodeId);
inline void UIBuilderEditScene(AppState& state, int sceneIndex) { UIBuilderEditLayout(state, sceneIndex, 0); }

// Older builds kept user UIs in chemlab_uis.toml; turn each into a scene
// file under scenes/ (once: the toml is renamed afterwards). Call before
// GraphSystem::LoadScenes.
void MigrateUserUIsToScenes(AppState& state);
