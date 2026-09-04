// Calculation Output: a per-frame table of what ChemLab knows about the
// active structure (energy from the comment line, every measurement), plus
// the composition of the current frame.

#include <cmath>
#include <limits>
#include <map>

#include <fmt/format.h>

#include "imgui.h"

#include "app/actions.h"
#include "ui/ui.h"

void DrawOutputPanel(AppState& state) {
    const Structure* s = state.ActiveStructure();
    const ChemicalData* atoms = state.ActiveChem();
    if (!s || !atoms) {
        ImGui::TextDisabled("No structure loaded.");
        return;
    }

    // ---- composition ----
    std::map<std::string, int> counts;
    for (uint32_t i = 0; i < atoms->natoms; ++i) counts[atoms->Label(i)]++;
    std::string formula;
    for (const auto& [el, n] : counts) formula += n > 1 ? fmt::format("{}{} ", el, n) : fmt::format("{} ", el);
    ImGui::Text("%s", s->name.c_str());
    ImGui::SameLine();
    ImGui::TextDisabled("frame %d/%u  |  %u atoms  |  %zu bonds  |  %s", s->activeFrame + 1, s->frames.nframes, atoms->natoms,
                        atoms->BondCount(), formula.c_str());
    ImGui::Separator();

    // ---- per-frame table ----
    const int nMeasure = (int)state.measurements.size();
    const int nCols = 2 + nMeasure;
    const ImGuiTableFlags flags = ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV |
                                  ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_Resizable;
    // The id carries the column count so ImGui does not try to reuse the saved
    // column order/widths of a differently shaped table.
    const std::string tableId = fmt::format("##frames{}", nCols);
    if (ImGui::BeginTable(tableId.c_str(), nCols, flags, ImVec2(0, 0))) {
        ImGui::TableSetupScrollFreeze(1, 1);
        ImGui::TableSetupColumn("Frame", ImGuiTableColumnFlags_WidthFixed, 60.0f);
        ImGui::TableSetupColumn("Energy", ImGuiTableColumnFlags_WidthFixed, 110.0f);
        for (const Measurement& m : state.measurements)
            ImGui::TableSetupColumn(MeasurementLabel(m).c_str(), ImGuiTableColumnFlags_WidthFixed, 110.0f);
        ImGui::TableHeadersRow();

        ImGuiListClipper clipper;
        clipper.Begin((int)s->frames.nframes);
        while (clipper.Step()) {
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::PushID(i);
                const bool active = i == s->activeFrame;
                if (ImGui::Selectable(fmt::format("{}", i + 1).c_str(), active, ImGuiSelectableFlags_SpanAllColumns))
                    SetFrame(state, i);
                ImGui::PopID();
                ImGui::TableNextColumn();
                const double e = i < (int)s->frames.energies.size() ? s->frames.energies[i] : std::numeric_limits<double>::quiet_NaN();
                if (!std::isnan(e)) ImGui::Text("%.6f", e);
                else ImGui::TextDisabled("-");
                for (const Measurement& m : state.measurements) {
                    ImGui::TableNextColumn();
                    const double v = MeasurementValue(s->frames.data[i], m);
                    ImGui::Text(m.count == 2 ? "%.4f" : "%.2f", v);
                }
            }
        }
        ImGui::EndTable();
    }
}
