#include "ui/ui.h"

#include <filesystem>

#include <fmt/format.h>

#include "imgui.h"
#include "imgui_internal.h"
#include "implot.h"
#include "implot3d.h"
#include "portable-file-dialogs.h"
#include "rlImGui.h"

#include "app/actions.h"
#include "ui/panel_registry.h"
#include "ui/theme.h"
#include "ui/ui_builder.h"

namespace {

bool gLayoutInitialised = false;
ImGuiID gDockspaceId = 0;

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
    state.uis = BuiltinUIs();
    LoadUserUIsIntoState(state);
    if (state.activeUI < 0 || state.activeUI >= (int)state.uis.size()) state.activeUI = 0;
    ApplyUIVisibility(state, state.uis[state.activeUI]);
    LogInfo(state, "ChemLab ready. Type `help` in the command bar (Ctrl+K) for the command list.");
}

void UIShutdown(AppState&) {
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
        if (state.resetLayoutRequested || ImGui::DockBuilderGetNode(gDockspaceId) == nullptr)
            ApplyUIDockLayout(state, ui, gDockspaceId, dockSize);
        if (state.resetLayoutRequested) {
            ApplyUIVisibility(state, ui);
            // Remember which UI the saved dock arrangement belongs to.
            SaveUserUIsFromState(state);
        }
        gLayoutInitialised = true;
        state.resetLayoutRequested = false;
    }
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
            if (ImGui::Begin(p.title, &open, panelFlags)) p.draw(state);
            ImGui::End();
            if (p.tightPadding) ImGui::PopStyleVar();
        }
    }
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
