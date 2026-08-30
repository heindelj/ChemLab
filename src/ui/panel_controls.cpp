// Controls: loading, frame playback, rendering, selection/colour and
// measurements. Everything here calls the same actions the command bar uses.

#include <algorithm>
#include <set>

#include <fmt/format.h>

#include "imgui.h"
#include "imgui_stdlib.h"

#include "app/actions.h"
#include "ui/ui.h"

namespace {

void FrameControls(AppState& state, Structure& s) {
    const int n = (int)s.frames.nframes;
    int frame = s.activeFrame + 1;
    ImGui::SetNextItemWidth(-90.0f);
    if (ImGui::SliderInt("##frame", &frame, 1, std::max(n, 1), "frame %d")) SetFrame(state, frame - 1);
    ImGui::SameLine();
    ImGui::Text("/ %d", n);

    if (ImGui::Button("|<")) SetFrame(state, 0);
    ImGui::SameLine();
    if (ImGui::Button("<")) StepFrame(state, -1);
    ImGui::SameLine();
    if (ImGui::Button(state.playback.playing ? "Pause" : "Play", ImVec2(60, 0))) {
        state.playback.playing = !state.playback.playing;
        state.playback.lastAdvance = GetTime();
    }
    ImGui::SameLine();
    if (ImGui::Button(">")) StepFrame(state, +1);
    ImGui::SameLine();
    if (ImGui::Button(">|")) SetFrame(state, n - 1);
    ImGui::SameLine();
    ImGui::Checkbox("Loop", &state.playback.loop);

    ImGui::SetNextItemWidth(120.0f);
    ImGui::DragFloat("fps", &state.playback.framesPerSecond, 0.5f, 0.5f, 120.0f, "%.1f");
    ImGui::SameLine();
    ImGui::Checkbox("Watch file", &state.watchFiles);
    HelpMarker("Reload the xyz file automatically when it changes on disk.");
    if (!s.frames.headers[s.activeFrame].empty()) {
        ImGui::TextDisabled("comment:");
        ImGui::SameLine();
        ImGui::TextWrapped("%s", s.frames.headers[s.activeFrame].c_str());
    }
}

void RenderingControls(AppState& state) {
    int style = (int)state.render.style;
    if (ImGui::Combo("Style", &style, "ball-and-stick\0spheres\0sticks\0")) {
        state.render.style = (RenderStyle)style;
        MarkGeometryChanged(state);
    }
    bool changed = false;
    if (state.render.style == RenderStyle::BallAndStick)
        changed |= ImGui::SliderFloat("Ball scale", &state.render.ballScale, 0.05f, 1.0f, "%.2f x vdW");
    if (state.render.style == RenderStyle::Spheres)
        changed |= ImGui::SliderFloat("Sphere scale", &state.render.sphereScale, 0.2f, 1.5f, "%.2f x vdW");
    if (state.render.style != RenderStyle::Spheres)
        changed |= ImGui::SliderFloat("Stick radius", &state.render.stickRadius, 0.03f, 0.6f, "%.2f A");
    ImGui::Checkbox("Colour bonds by atom", &state.render.colorBonds);
    if (changed) MarkGeometryChanged(state);

    float bg[3] = {state.background.r / 255.0f, state.background.g / 255.0f, state.background.b / 255.0f};
    if (ImGui::ColorEdit3("Background", bg, ImGuiColorEditFlags_NoInputs))
        state.background = Color{(unsigned char)(bg[0] * 255), (unsigned char)(bg[1] * 255), (unsigned char)(bg[2] * 255), 255};
    ImGui::Checkbox("Grid", &state.drawGrid);
    ImGui::SameLine();
    ImGui::Checkbox("Atom numbers", &state.drawAtomNumbers);
    ImGui::SameLine();
    ImGui::Checkbox("Labels", &state.drawMeasurements);
    ImGui::Checkbox("Auto-rotate", &state.autoRotate);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100.0f);
    ImGui::DragFloat("deg/s", &state.autoRotateDegPerSec, 1.0f, -360.0f, 360.0f, "%.0f");
    if (ImGui::Button("Reset camera")) ResetCamera(state);
}

void SelectionControls(AppState& state, const Atoms& atoms) {
    ImGui::Text("%zu of %u atoms selected", state.selected.size(), atoms.natoms);
    HelpMarker("Shift-click atoms in the 3D view, or use the `select` command.");
    if (ImGui::SmallButton("All")) SelectAll(state);
    ImGui::SameLine();
    if (ImGui::SmallButton("None")) SelectNone(state);
    ImGui::SameLine();
    if (ImGui::SmallButton("Invert")) InvertSelection(state);
    ImGui::SameLine();
    // Element picker built from what is actually in the frame.
    std::set<std::string> elements(atoms.labels.begin(), atoms.labels.end());
    ImGui::SetNextItemWidth(90.0f);
    if (ImGui::BeginCombo("##element", "element...")) {
        for (const std::string& e : elements)
            if (ImGui::Selectable(e.c_str())) SelectByElement(state, e, ImGui::GetIO().KeyShift);
        ImGui::EndCombo();
    }

    ImGui::ColorEdit4("##picker", state.pickerColor, ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoInputs);
    ImGui::SameLine();
    if (ImGui::Button("Apply colour")) ColorSelection(state, ImVec4ToColor(state.pickerColor));
    ImGui::SameLine();
    if (ImGui::Button("Reset colours")) ResetColors(state);
}

void MeasurementControls(AppState& state, const Atoms& atoms) {
    ImGui::TextWrapped("Click two, three or four atoms in the 3D view for a distance, angle or dihedral. "
                       "Enter keeps a partial selection, Escape cancels.");
    if (state.measurements.empty()) {
        ImGui::TextDisabled("No measurements yet.");
        return;
    }
    if (ImGui::BeginTable("##measurements", 3, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Measurement", ImGuiTableColumnFlags_WidthStretch, 2.0f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 1.2f);
        ImGui::TableSetupColumn("##x", ImGuiTableColumnFlags_WidthFixed, 22.0f);
        ImGui::TableHeadersRow();
        int toDelete = -1;
        for (size_t i = 0; i < state.measurements.size(); ++i) {
            const Measurement& m = state.measurements[i];
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(MeasurementLabel(m).c_str());
            ImGui::TableNextColumn();
            const double v = MeasurementValue(atoms, m);
            ImGui::Text(m.count == 2 ? "%.4f A" : "%.2f deg", v);
            ImGui::TableNextColumn();
            ImGui::PushID((int)i);
            if (ImGui::SmallButton("x")) toDelete = (int)i;
            ImGui::PopID();
        }
        ImGui::EndTable();
        if (toDelete >= 0) RemoveMeasurement(state, toDelete);
    }
    if (ImGui::SmallButton("Clear all")) ClearMeasurements(state);
}

}  // namespace

void DrawControlsPanel(AppState& state) {
    if (state.project) {
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Project: %s%s", state.project->config.name.c_str(), state.projectDirty ? " *" : "");
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", state.project->Root().string().c_str());
        ImGui::SameLine();
        if (ImGui::SmallButton("Save")) RunCommandLine(state, "project save");
    } else {
        ImGui::TextDisabled("No project (File > New project to keep this session)");
    }
    if (ImGui::Button("Open xyz...", ImVec2(140, 0))) {
        std::vector<std::string> paths;
        if (OpenFileDialog("Open geometry", paths, true))
            for (const auto& p : paths) RunCommandLine(state, fmt::format("load \"{}\"", p));
    }
    Structure* s = state.ActiveStructure();
    ImGui::SameLine();
    ImGui::AlignTextToFramePadding();
    ImGui::TextDisabled("Active: %s", s ? s->name.c_str() : "-");
    ImGui::Separator();
    ImGui::Spacing();

    if (!s) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        ImGui::TextWrapped("Open an xyz file to get started. Multiple files can be loaded; pick the active one in the "
                           "Active Structure panel.");
        ImGui::PopStyleColor();
        return;
    }
    const Atoms* atoms = state.ActiveAtoms();

    if (ImGui::CollapsingHeader("Frames", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Indent(4.0f);
        FrameControls(state, *s);
        ImGui::Unindent(4.0f);
        ImGui::Spacing();
    }
    if (ImGui::CollapsingHeader("Rendering", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Indent(4.0f);
        ImGui::PushItemWidth(150.0f);
        RenderingControls(state);
        ImGui::PopItemWidth();
        ImGui::Unindent(4.0f);
        ImGui::Spacing();
    }
    if (atoms && ImGui::CollapsingHeader("Selection & colour", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Indent(4.0f);
        SelectionControls(state, *atoms);
        ImGui::Unindent(4.0f);
        ImGui::Spacing();
    }
    if (atoms && ImGui::CollapsingHeader("Measurements", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Indent(4.0f);
        MeasurementControls(state, *atoms);
        ImGui::Unindent(4.0f);
    }
}
