// Active Structure: the list of loaded structures. Exactly one is active;
// right-click for rename / remove, like quick-mag's panel of the same name.

#include <fmt/format.h>

#include "imgui.h"
#include "imgui_stdlib.h"

#include "app/actions.h"
#include "ui/ui.h"

namespace {
int gRenameIndex = -1;
std::string gRenameBuffer;
}  // namespace

void DrawActiveStructurePanel(AppState& state) {
    if (ImGui::Button("Open xyz...")) {
        std::vector<std::string> paths;
        if (OpenFileDialog("Open geometry", paths, true))
            for (const auto& p : paths) RunCommandLine(state, fmt::format("load \"{}\"", p));
    }
    ImGui::SameLine();
    ImGui::TextDisabled("%zu loaded", state.structures.size());
    ImGui::Separator();

    int pendingDelete = -1;
    for (size_t i = 0; i < state.structures.size(); ++i) {
        const Structure& s = state.structures[i];
        ImGui::PushID((int)i);
        const bool active = (int)i == state.activeStructure;
        const std::string label = fmt::format("{}  ({} frame{}, {} atoms)", s.name, s.frames.nframes,
                                              s.frames.nframes == 1 ? "" : "s", s.frames.data.empty() ? 0 : s.frames.data[0].natoms);
        if (ImGui::Selectable(label.c_str(), active, ImGuiSelectableFlags_AllowDoubleClick)) SetActiveStructure(state, (int)i);
        if (ImGui::IsItemHovered() && !s.path.empty()) ImGui::SetTooltip("%s", s.path.c_str());
        if (ImGui::BeginPopupContextItem("structure_context")) {
            if (ImGui::MenuItem("Make active")) SetActiveStructure(state, (int)i);
            if (ImGui::MenuItem("Rename")) {
                gRenameIndex = (int)i;
                gRenameBuffer = s.name;
            }
            if (ImGui::MenuItem("Reload from disk", nullptr, false, !s.path.empty())) {
                const std::string path = s.path;
                pendingDelete = (int)i;
                RunCommandLine(state, fmt::format("load \"{}\"", path));
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Remove")) pendingDelete = (int)i;
            ImGui::EndPopup();
        }
        ImGui::PopID();
    }
    if (state.structures.empty()) ImGui::TextDisabled("Nothing loaded yet.");

    if (pendingDelete >= 0) RemoveStructure(state, pendingDelete);

    if (gRenameIndex >= 0) {
        ImGui::OpenPopup("Rename structure");
        if (ImGui::BeginPopupModal("Rename structure", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::SetNextItemWidth(260.0f);
            if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
            const bool enter = ImGui::InputText("##name", &gRenameBuffer, ImGuiInputTextFlags_EnterReturnsTrue);
            if (ImGui::Button("OK") || enter) {
                if (gRenameIndex < (int)state.structures.size()) RenameStructure(state, gRenameIndex, gRenameBuffer);
                gRenameIndex = -1;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) {
                gRenameIndex = -1;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }
}
