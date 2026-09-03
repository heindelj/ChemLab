#pragma once
// The ImGui side of ChemLab: dock layout, menu bar, panels and command bar.
// raylib owns the window; rlImGui is the backend. Call UIInit once after
// InitWindow, UIFrame every frame between BeginDrawing/EndDrawing, and
// UIShutdown before CloseWindow.

#include "app/app_state.h"

void UIInit(AppState& state);
void UIFrame(AppState& state);
void UIShutdown(AppState& state);

// Panel entry points (one per docked window).
void DrawControlsPanel(AppState& state);
void DrawStructureViewPanel(AppState& state);
void DrawPlotPanel(AppState& state);
void DrawActiveStructurePanel(AppState& state);
void DrawCalculatePanel(AppState& state);
void DrawOutputPanel(AppState& state);
void DrawExportPanel(AppState& state);
void DrawNodeGraphPanel(AppState& state);
void DrawGraphCanvasPanel(AppState& state);   // the Graph Canvas: sketch, run, save/load by name
// The "Graph: <panel>" windows (state.graphViewOpen), one per panel graph.
void DrawPanelGraphWindows(AppState& state);
void NodeGraphShutdown();   // destroys the node editor contexts (UIShutdown)

// Bottom command bar and the optional console window.
float CommandBarHeight();
void DrawCommandBar(AppState& state);
void DrawConsolePanel(AppState& state);
void DrawInputDebugOverlay(AppState& state);
// Run a command line through the registry, logging it and its result.
void RunCommandLine(AppState& state, const std::string& line);

// Shared helpers.
void HelpMarker(const char* text);
bool OpenFileDialog(const char* title, std::vector<std::string>& outPaths, bool multiple);
bool SelectFolderDialog(const char* title, std::string& outPath);
bool SaveFileDialog(const char* title, const std::string& defaultName, std::string& outPath);

// Panel titles (also used as dock window names)
namespace PanelName {
inline constexpr const char* Controls = "Controls";
inline constexpr const char* StructureView = "Structure View";
inline constexpr const char* Plot = "2D Plot";
inline constexpr const char* ActiveStructure = "Active Structure";
inline constexpr const char* Calculate = "Calculate";
inline constexpr const char* Output = "Calculation Output";
inline constexpr const char* Export = "Export";
inline constexpr const char* Console = "Console";
inline constexpr const char* NodeGraph = "Node Graph";
inline constexpr const char* GraphCanvas = "Graph Canvas";
}  // namespace PanelName
