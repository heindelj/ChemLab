// Export: screenshots and xyz files.

#include <fmt/format.h>

#include "imgui.h"
#include "imgui_stdlib.h"

#include "app/actions.h"
#include "ui/ui.h"

void DrawExportPanel(AppState& state) {
    const bool loaded = state.ActiveStructure() != nullptr;
    ExportSettings& ex = state.exportSettings;

    ImGui::SeparatorText("Image");
    ImGui::PushItemWidth(90.0f);
    ImGui::InputInt("W", &ex.screenshotWidth, 0);
    ImGui::SameLine();
    ImGui::InputInt("H", &ex.screenshotHeight, 0);
    ImGui::PopItemWidth();
    ImGui::SameLine();
    ImGui::Checkbox("Transparent", &ex.transparentBackground);
    if (ex.screenshotWidth < 16) ex.screenshotWidth = 16;
    if (ex.screenshotHeight < 16) ex.screenshotHeight = 16;
    ImGui::BeginDisabled(!loaded);
    if (ImGui::Button("Save screenshot...", ImVec2(-1, 0))) {
        std::string path;
        if (SaveFileDialog("Save screenshot", "screenshot.png", path))
            RunCommandLine(state, fmt::format("screenshot \"{}\" --size {}x{}{}", path, ex.screenshotWidth, ex.screenshotHeight,
                                              ex.transparentBackground ? " --transparent" : ""));
    }
    ImGui::EndDisabled();
    if (!ex.lastScreenshotPath.empty()) ImGui::TextDisabled("last: %s", ex.lastScreenshotPath.c_str());

    ImGui::Spacing();
    ImGui::SeparatorText("Geometry");
    ImGui::BeginDisabled(!loaded);
    if (ImGui::Button("Export current frame (.xyz)...", ImVec2(-1, 0))) {
        std::string path;
        if (SaveFileDialog("Export xyz", "frame.xyz", path)) RunCommandLine(state, fmt::format("export \"{}\"", path));
    }
    if (ImGui::Button("Export all frames (.xyz)...", ImVec2(-1, 0))) {
        std::string path;
        if (SaveFileDialog("Export xyz", "trajectory.xyz", path)) RunCommandLine(state, fmt::format("export \"{}\" --all", path));
    }
    ImGui::EndDisabled();
    if (!ex.lastXYZPath.empty()) ImGui::TextDisabled("last: %s", ex.lastXYZPath.c_str());
}
