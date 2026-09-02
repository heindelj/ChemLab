#include "ui/ui.h"

#include <cmath>
#include <filesystem>

#include <fmt/format.h>

#include "imgui.h"
#include "imgui_internal.h"
#include "implot.h"
#include "implot3d.h"
#if !defined(__EMSCRIPTEN__)
#include "portable-file-dialogs.h"   // shells out to zenity/Cocoa/Win32: native only
#endif
#include "rlImGui.h"

#include "app/actions.h"
#include "graph/graph_system.h"
#include "ui/panel_registry.h"
#include "ui/theme.h"
#include "ui/ui_builder.h"

namespace {

bool gLayoutInitialised = false;
bool gPlotPanelMigrated = false;
int gFramesSinceLayoutLoad = 0;
ImGuiID gDockspaceId = 0;

// Share of the centre column given to the 2D plot, matching the fixed
// splitter it replaced (the old AppState::twoDPaneFraction default).
constexpr float kPlotPaneFraction = 0.30f;

// The 2D plot used to be drawn inside the Structure View panel. A dock layout
// saved by an older build knows nothing about the new "2D Plot" window, which
// would leave it floating; dock it under the 3D view at the same share of the
// column the old splitter used, leaving the rest of the saved arrangement
// alone. (View > Reset layout rebuilds everything from the active UI.)
//
// This deliberately runs a frame late: the nodes restored from the ini file
// only get their real sizes once the dockspace has been submitted, and
// splitting a zero-sized node yields a zero SizeRef, which is what made the
// new panel come up as a sliver.
void MigrateSavedLayoutForPlotPanel(ImGuiID dockspaceId, ImVec2 dockSize) {
    if (gPlotPanelMigrated) return;
    if (gFramesSinceLayoutLoad++ < 1) return;
    gPlotPanelMigrated = true;
    if (ImGui::FindWindowSettingsByID(ImHashStr(PanelName::Plot))) return;   // layout already knows it
    ImGuiWindowSettings* view = ImGui::FindWindowSettingsByID(ImHashStr(PanelName::StructureView));
    if (!view || view->DockId == 0) return;
    ImGuiDockNode* node = ImGui::DockBuilderGetNode(view->DockId);
    if (!node) return;

    ImVec2 size = node->Size;
    if (size.x < 1.0f) size.x = std::fmax(view->Size.x, dockSize.x);
    if (size.y < 1.0f) size.y = std::fmax(view->Size.y, dockSize.y);
    if (size.x < 1.0f || size.y < 1.0f) return;

    ImGuiID below = 0, above = 0;
    ImGui::DockBuilderSplitNode(view->DockId, ImGuiDir_Down, kPlotPaneFraction, &below, &above);
    // Split ratios alone are unreliable here; state the sizes outright.
    ImGui::DockBuilderSetNodeSize(above, ImVec2(size.x, std::fmax(size.y * (1.0f - kPlotPaneFraction), 1.0f)));
    ImGui::DockBuilderSetNodeSize(below, ImVec2(size.x, std::fmax(size.y * kPlotPaneFraction, 1.0f)));
    ImGui::DockBuilderDockWindow(PanelName::Plot, below);
    ImGui::DockBuilderFinish(dockspaceId);
}

// The active UIDefinition is applied through ApplyUIDockLayout (ui_builder.h);
// the old hard-coded dock layout is now the built-in "Default" UI in ui_spec.cpp.
const UIDefinition& ActiveUI(AppState& state) {
    if (state.uis.empty()) state.uis = BuiltinUIs();
    if (state.activeUI < 0 || state.activeUI >= (int)state.uis.size()) state.activeUI = 0;
    return state.uis[state.activeUI];
}

void DrawMenuBar(AppState& state) {
    if (!ImGui::BeginMainMenuBar()) return;
    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("New project...")) {
            std::string dir;
            if (SelectFolderDialog("Choose a folder for the new project", dir)) RunCommandLine(state, fmt::format("project new \"{}\"", dir));
        }
        if (ImGui::MenuItem("Open project...")) {
            std::string dir;
            if (SelectFolderDialog("Open a project folder (containing chemlab.toml)", dir))
                RunCommandLine(state, fmt::format("project open \"{}\"", dir));
        }
        if (ImGui::MenuItem("Save project", "Ctrl+Shift+S", false, state.project.has_value())) RunCommandLine(state, "project save");
        if (ImGui::MenuItem("Close project", nullptr, false, state.project.has_value())) RunCommandLine(state, "project close");
        ImGui::Separator();
        if (ImGui::MenuItem("Open xyz...", "Ctrl+O")) {
            std::vector<std::string> paths;
            if (OpenFileDialog("Open geometry", paths, true))
                for (const auto& p : paths) RunCommandLine(state, fmt::format("load \"{}\"", p));
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Save screenshot...", "Ctrl+S", false, state.model.IsLoaded())) {
            std::string path;
            if (SaveFileDialog("Save screenshot", "screenshot.png", path))
                RunCommandLine(state, fmt::format("screenshot \"{}\"", path));
        }
        if (ImGui::MenuItem("Export xyz (current frame)...", nullptr, false, state.ActiveStructure() != nullptr)) {
            std::string path;
            if (SaveFileDialog("Export xyz", "frame.xyz", path)) RunCommandLine(state, fmt::format("export \"{}\"", path));
        }
        if (ImGui::MenuItem("Export xyz (all frames)...", nullptr, false, state.ActiveStructure() != nullptr)) {
            std::string path;
            if (SaveFileDialog("Export xyz", "trajectory.xyz", path)) RunCommandLine(state, fmt::format("export \"{}\" --all", path));
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Quit", "Ctrl+Q")) state.quitRequested = true;
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("View")) {
        for (const PanelInfo& p : PanelCatalog())
            ImGui::MenuItem(p.title, std::string(p.id) == "console" ? "Ctrl+`" : nullptr, &state.PanelOpen(p.id));
        ImGui::Separator();
        if (ImGui::BeginMenu("Interface")) {
            for (int i = 0; i < (int)state.uis.size(); ++i)
                if (ImGui::MenuItem(state.uis[i].name.c_str(), nullptr, state.activeUI == i)) {
                    state.activeUI = i;
                    state.resetLayoutRequested = true;
                }
            ImGui::Separator();
            if (ImGui::MenuItem("UI Builder...")) state.uiBuilder.open = true;
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Panel graphs")) {
            for (const PanelInfo& p : PanelCatalog()) {
                if (std::string(p.id) == "node_graph") continue;
                ImGui::MenuItem(p.title, nullptr, &state.graphViewOpen[p.id]);
            }
            ImGui::EndMenu();
        }
        ImGui::Separator();
        ImGui::MenuItem("Grid", "G", &state.drawGrid);
        ImGui::MenuItem("Atom numbers", "N", &state.drawAtomNumbers);
        ImGui::MenuItem("Measurement labels", nullptr, &state.drawMeasurements);
        ImGui::Separator();
        if (ImGui::MenuItem("Reset camera", "R")) ResetCamera(state);
        if (ImGui::MenuItem("Reset layout")) state.resetLayoutRequested = true;
        ImGui::Separator();
        if (ImGui::BeginMenu("Theme")) {
            for (UITheme t : {UITheme::Nord, UITheme::Mocha, UITheme::Darcula})
                if (ImGui::MenuItem(UIThemeName(t), nullptr, state.theme == t)) RunCommandLine(state, fmt::format("theme {}", UIThemeName(t)));
            ImGui::EndMenu();
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Help")) {
        if (ImGui::MenuItem("Command reference")) {
            state.PanelOpen("console") = true;
            RunCommandLine(state, "help");
        }
        ImGui::Separator();
        ImGui::MenuItem("ImGui demo", nullptr, &state.showImGuiDemo);
        ImGui::MenuItem("ImPlot demo", nullptr, &state.showImPlotDemo);
        ImGui::MenuItem("Metrics", nullptr, &state.showMetrics);
        ImGui::EndMenu();
    }
    // Right-aligned frame rate
    const std::string fps = fmt::format("{:.0f} fps", ImGui::GetIO().Framerate);
    ImGui::SameLine(ImGui::GetWindowWidth() - ImGui::CalcTextSize(fps.c_str()).x - 12.0f);
    ImGui::TextDisabled("%s", fps.c_str());
    ImGui::EndMainMenuBar();
}

// Right-clicking a panel's tab (or, when floating, its title bar) opens a
// small menu with "View graph": every panel is backed by a node graph (see
// graph_system.h) and this is how it is reached. Call right after
// ImGui::Begin() of the panel window, while it is the current window.
void PanelTitleContextMenu(AppState& state, const PanelInfo& p, bool& open) {
    ImGuiWindow* w = ImGui::GetCurrentWindow();
    ImRect rect;
    bool haveRect = false;
    if (w->DockIsActive && w->DockNode && w->DockNode->TabBar) {
        ImGuiTabBar* tb = w->DockNode->TabBar;
        if (ImGuiTabItem* tab = ImGui::TabBarFindTabByID(tb, w->TabId)) {
            const float x0 = tb->BarRect.Min.x + tab->Offset - tb->ScrollingAnim;
            rect = ImRect(ImVec2(x0, tb->BarRect.Min.y), ImVec2(x0 + tab->Width, tb->BarRect.Max.y));
            rect.ClipWith(tb->BarRect);
            haveRect = true;
        }
    } else if (!w->DockIsActive && !(w->Flags & ImGuiWindowFlags_NoTitleBar)) {
        rect = w->TitleBarRect();
        haveRect = true;
    }
    const std::string popupId = fmt::format("##panel_ctx_{}", p.id);
    if (haveRect && ImGui::IsMouseClicked(ImGuiMouseButton_Right) && rect.Contains(ImGui::GetMousePos()) &&
        !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId))
        ImGui::OpenPopup(popupId.c_str());
    if (ImGui::BeginPopup(popupId.c_str())) {
        ImGui::TextDisabled("%s", p.title);
        ImGui::Separator();
        const bool hasGraph = std::string(p.id) != "node_graph";
        if (ImGui::MenuItem("View graph", nullptr, false, hasGraph)) state.graphViewOpen[p.id] = true;
        if (ImGui::MenuItem("Reset graph to default", nullptr, false, hasGraph)) state.GraphSys().ResetPanel(state, p.id);
        ImGui::Separator();
        if (ImGui::MenuItem("Close panel")) open = false;
        ImGui::EndPopup();
    }
}

void HandleGlobalShortcuts(AppState& state) {
    ImGuiIO& io = ImGui::GetIO();
    const bool ctrl = io.KeyCtrl || io.KeySuper;
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_K, false)) state.focusCommandBar = true;
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_GraveAccent, false)) state.PanelOpen("console") = !state.PanelOpen("console");
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Q, false)) state.quitRequested = true;
    if (ctrl && io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_S, false) && state.project) RunCommandLine(state, "project save");
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_O, false)) {
        std::vector<std::string> paths;
        if (OpenFileDialog("Open geometry", paths, true))
            for (const auto& p : paths) RunCommandLine(state, fmt::format("load \"{}\"", p));
    }
    if (io.WantTextInput) return;  // the rest are bare keys
    if (ImGui::IsKeyPressed(ImGuiKey_RightArrow, true)) StepFrame(state, +1);
    if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow, true)) StepFrame(state, -1);
    if (ImGui::IsKeyPressed(ImGuiKey_Space, false)) {
        state.playback.playing = !state.playback.playing;
        state.playback.lastAdvance = GetTime();
    }
    if (ImGui::IsKeyPressed(ImGuiKey_G, false)) state.drawGrid = !state.drawGrid;
    if (ImGui::IsKeyPressed(ImGuiKey_N, false)) state.drawAtomNumbers = !state.drawAtomNumbers;
    if (ImGui::IsKeyPressed(ImGuiKey_R, false)) ResetCamera(state);
    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
        CancelPendingMeasurement(state);
        state.selected.clear();
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false))
        CommitPendingMeasurement(state);
}

}  // namespace

// ---------------------------------------------------------------------------
void UIInit(AppState& state) {
    rlImGuiSetLoadFontsCallback([]() { LoadChemLabFonts(16.0f); });
    rlImGuiBeginInitImGui();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigDockingWithShift = false;
    io.IniFilename = state.iniFileName.c_str();
    rlImGuiEndInitImGui();
    ImPlot::CreateContext();
    ImPlot3D::CreateContext();
    ApplyChemLabTheme(state.theme);
    RegisterBuiltinCommands(state.commands);
    RegisterPanelNodes();   // "panel.<id>" wrapper node types, before any panel graph is seeded
    state.uis = BuiltinUIs();
    LoadUserUIsIntoState(state);
    if (state.activeUI < 0 || state.activeUI >= (int)state.uis.size()) state.activeUI = 0;
    ApplyUIVisibility(state, state.uis[state.activeUI]);
    LogInfo(state, "ChemLab ready. Type `help` in the command bar (Ctrl+K) for the command list.");
}

void UIShutdown(AppState&) {
    NodeGraphShutdown();
    ImPlot3D::DestroyContext();
    ImPlot::DestroyContext();
    rlImGuiShutdown();
}

// Switch ImGui's settings file (dock layout etc.). Must run between frames.
static void ApplyPendingIniFile(AppState& state) {
    if (state.pendingIniFile.empty()) return;
    ImGuiIO& io = ImGui::GetIO();
    if (io.IniFilename) ImGui::SaveIniSettingsToDisk(io.IniFilename);
    state.iniFileName = state.pendingIniFile;
    state.pendingIniFile.clear();
    io.IniFilename = state.iniFileName.c_str();
    ImGui::ClearIniSettings();
    if (std::filesystem::exists(state.iniFileName)) {
        ImGui::LoadIniSettingsFromDisk(state.iniFileName.c_str());
        gLayoutInitialised = false;   // re-check that the dockspace exists
        gPlotPanelMigrated = false;   // ...and that this layout knows the 2D Plot panel
        gFramesSinceLayoutLoad = 0;
    } else {
        state.resetLayoutRequested = true;
    }
}

void UIFrame(AppState& state) {
    ApplyPendingIniFile(state);
    rlImGuiBegin();
    HandleGlobalShortcuts(state);
    DrawMenuBar(state);

    // Host window for the dockspace: everything except the menu bar and the
    // command bar at the bottom of the screen.
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    const float barHeight = CommandBarHeight();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(ImVec2(vp->WorkSize.x, vp->WorkSize.y - barHeight));
    ImGui::SetNextWindowViewport(vp->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    const ImGuiWindowFlags hostFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
                                       ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
                                       ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoBackground;
    ImGui::Begin("##DockHost", nullptr, hostFlags);
    ImGui::PopStyleVar(3);
    gDockspaceId = ImGui::GetID("ChemLabDockSpace");
    const ImVec2 dockSize = ImGui::GetContentRegionAvail();
    if (!gLayoutInitialised || state.resetLayoutRequested) {
        // Only build the layout ourselves if there is no saved one (or on request).
        const UIDefinition& ui = ActiveUI(state);
        if (state.resetLayoutRequested || ImGui::DockBuilderGetNode(gDockspaceId) == nullptr) {
            ApplyUIDockLayout(state, ui, gDockspaceId, dockSize);
            gPlotPanelMigrated = true;   // a freshly built layout already has the 2D Plot panel
        }
        if (state.resetLayoutRequested) {
            ApplyUIVisibility(state, ui);
            // Remember which UI the saved dock arrangement belongs to.
            SaveUserUIsFromState(state);
        }
        gLayoutInitialised = true;
        state.resetLayoutRequested = false;
    }
    MigrateSavedLayoutForPlotPanel(gDockspaceId, dockSize);
    UIBuilderPreDockspace(state, gDockspaceId, dockSize);
    ImGui::DockSpace(gDockspaceId, dockSize, ImGuiDockNodeFlags_None);
    ImGui::End();

    // Panels are drawn generically from the registry. While the UI builder's
    // edit mode is active the real panels hide and slot placeholders render
    // in their place (see DrawUIBuilder).
    const ImGuiWindowFlags panelFlags = ImGuiWindowFlags_NoCollapse;
    if (!state.uiBuilder.editing) {
        for (const PanelInfo& p : PanelCatalog()) {
            bool& open = state.PanelOpen(p.id);
            if (!open) continue;
            if (p.tightPadding) ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 4));
            if (ImGui::Begin(p.title, &open, panelFlags)) {
                PanelTitleContextMenu(state, p, open);
                p.draw(state);
            }
            ImGui::End();
            if (p.tightPadding) ImGui::PopStyleVar();
        }
    }
    DrawPanelGraphWindows(state);
    DrawUIBuilder(state);
    if (state.showImGuiDemo) ImGui::ShowDemoWindow(&state.showImGuiDemo);
    if (state.showImPlotDemo) ImPlot::ShowDemoWindow(&state.showImPlotDemo);
    if (state.showMetrics) ImGui::ShowMetricsWindow(&state.showMetrics);

    DrawCommandBar(state);
    if (state.showInputDebug) DrawInputDebugOverlay(state);
    rlImGuiEnd();
    if (state.showInputDebug) {
        // Drawn with raylib, after ImGui: a red crosshair where raylib thinks
        // the mouse is. The green ImGui circle should sit exactly on top of it.
        const Vector2 m = GetMousePosition();
        DrawLine((int)m.x - 15, (int)m.y, (int)m.x + 15, (int)m.y, RED);
        DrawLine((int)m.x, (int)m.y - 15, (int)m.x, (int)m.y + 15, RED);
    }
}

void DrawInputDebugOverlay(AppState& state) {
    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    dl->AddCircle(io.MousePos, 10.0f, IM_COL32(80, 255, 80, 255), 0, 2.0f);
    const Vector2 dpi = GetWindowScaleDPI();
    const Vector2 rm = GetMousePosition();
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, 30.0f), ImGuiCond_Always, ImVec2(0.5f, 0.0f));
    ImGui::SetNextWindowBgAlpha(0.85f);
    if (ImGui::Begin("##inputdebug", &state.showInputDebug,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
                         ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav)) {
        ImGui::Text("raylib mouse  (%.0f, %.0f)   ImGui mouse (%.0f, %.0f)", rm.x, rm.y, io.MousePos.x, io.MousePos.y);
        ImGui::Text("screen %dx%d  render %dx%d  dpi %.2f  fb scale %.2f  DisplaySize %.0fx%.0f", GetScreenWidth(),
                    GetScreenHeight(), GetRenderWidth(), GetRenderHeight(), dpi.x, io.DisplayFramebufferScale.x, io.DisplaySize.x,
                    io.DisplaySize.y);
        ImGui::TextDisabled("red cross = raylib, green ring = ImGui; they should coincide with the real cursor. `debug input off` hides this.");
    }
    ImGui::End();
}

// ---------------------------------------------------------------------------
void HelpMarker(const char* text) {
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::BeginItemTooltip()) {
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 30.0f);
        ImGui::TextUnformatted(text);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

#if defined(__EMSCRIPTEN__)

// The browser has no synchronous native file picker, and portable-file-dialogs
// works by spawning a helper process, which Emscripten cannot do. Files reach
// the web build by being dropped onto the canvas (Emscripten's GLFW writes
// them into the virtual filesystem and raylib reports them through
// IsFileDropped) or by being preloaded from assets/.
bool OpenFileDialog(const char*, std::vector<std::string>& outPaths, bool) {
    outPaths.clear();
    return false;
}

bool SelectFolderDialog(const char*, std::string& outPath) {
    outPath.clear();
    return false;
}

bool SaveFileDialog(const char*, const std::string&, std::string& outPath) {
    outPath.clear();
    return false;
}

#else

bool OpenFileDialog(const char* title, std::vector<std::string>& outPaths, bool multiple) {
    auto dialog = pfd::open_file(title, ".", {"Geometry files", "*.xyz", "All files", "*"},
                                 multiple ? pfd::opt::multiselect : pfd::opt::none);
    outPaths = dialog.result();
    return !outPaths.empty();
}

bool SelectFolderDialog(const char* title, std::string& outPath) {
    outPath = pfd::select_folder(title, ".").result();
    return !outPath.empty();
}

bool SaveFileDialog(const char* title, const std::string& defaultName, std::string& outPath) {
    outPath = pfd::save_file(title, defaultName, {"All files", "*"}).result();
    return !outPath.empty();
}

#endif
