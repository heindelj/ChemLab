// Calculate: the workflow / solver-settings panel. Today it holds the only
// "calculation" ChemLab does (bond perception); the layout is the one the
// future optimisation / MD / electronic-structure workflows will slot into.

#include "imgui.h"

#include "app/actions.h"
#include "ui/ui.h"

void DrawCalculatePanel(AppState& state) {
    const Structure* s = state.ActiveStructure();
    ImGui::TextDisabled("Target: %s", s ? s->name.c_str() : "-");
    ImGui::Spacing();

    ImGui::Text("Workflow");
    ImGui::Separator();
    ImGui::BulletText("Load a structure (or trajectory)");
    ImGui::BulletText("Perceive bonds from covalent radii");
    ImGui::BulletText("Measure geometry across frames");
    ImGui::BulletText("Export frames / images");
    ImGui::Spacing();

    if (ImGui::CollapsingHeader("Bond perception", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::PushItemWidth(160.0f);
        ImGui::SliderFloat("Tolerance (A)", &state.calc.bondTolerance, 0.0f, 1.0f, "%.2f");
        HelpMarker("Two atoms are bonded when their distance is below the sum of their covalent radii plus this tolerance.");
        ImGui::PopItemWidth();
        if (ImGui::Button("Recompute bonds", ImVec2(160, 0))) RunCommandLine(state, "bonds");
        if (s) {
            const ChemicalData* a = state.ActiveChem();
            ImGui::SameLine();
            ImGui::TextDisabled("%zu bonds in this frame", a ? a->BondCount() : 0);
        }
    }

    if (ImGui::CollapsingHeader("Coming soon")) {
        ImGui::BeginDisabled();
        ImGui::Button("Geometry optimisation", ImVec2(-1, 0));
        ImGui::Button("Single point energy", ImVec2(-1, 0));
        ImGui::Button("Molecular dynamics", ImVec2(-1, 0));
        ImGui::Button("Normal modes", ImVec2(-1, 0));
        ImGui::EndDisabled();
        ImGui::TextWrapped("Backends will register their own commands in the command registry, so anything added here is "
                           "also scriptable from the command bar.");
    }
}
