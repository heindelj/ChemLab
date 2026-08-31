// 2D Plot panel: energy or measurements against frame number (ImPlot).
// Used to live in the bottom half of the Structure View panel; it is now a
// dockable panel of its own, so the split between it and the 3D view is a
// normal dock splitter and either one can be resized, tabbed or closed.

#include <algorithm>
#include <cmath>
#include <limits>

#include "imgui.h"
#include "implot.h"

#include "app/actions.h"
#include "core/math_utils.h"
#include "ui/ui.h"

namespace {

const char* kPlotNames[] = {"Energy per frame", "Measurements per frame"};

// Above this many points, markers and the NaN-skipping line style get too
// slow / too dense to be useful; draw a plain decimated-looking line instead.
constexpr int kMaxMarkerPoints = 1500;

void PlotEnergy(AppState& state, const Structure& s) {
    // Cached at load: parsing 15k headers per rendered frame killed large files.
    const std::vector<double>& energies = s.frames.energies;
    static std::vector<double> x;   // shared scratch, rebuilt only on size change
    if (x.size() != energies.size()) {
        x.resize(energies.size());
        for (size_t i = 0; i < x.size(); ++i) x[i] = (double)(i + 1);
    }
    if (ImPlot::BeginPlot("##energy", ImVec2(-1, -1), ImPlotFlags_NoMouseText | ImPlotFlags_NoLegend)) {
        ImPlot::SetupAxes("Frame", "Energy (from comment line)");
        ImPlot::SetupAxisLimits(ImAxis_X1, 0.5, std::fmax(1.5, s.frames.nframes + 0.5), ImGuiCond_Always);
        if (s.frames.anyEnergy) {
            double lo = std::numeric_limits<double>::max(), hi = -lo;
            for (double e : energies)
                if (!std::isnan(e)) { lo = std::fmin(lo, e); hi = std::fmax(hi, e); }
            const double pad = std::fmax((hi - lo) * 0.08, 1e-6);
            ImPlot::SetupAxisLimits(ImAxis_Y1, lo - pad, hi + pad, ImGuiCond_Once);
            ImPlotSpec spec;
            if ((int)energies.size() <= kMaxMarkerPoints) {
                spec.Marker = ImPlotMarker_Circle;
                spec.MarkerSize = 4.0f;
            }
            spec.Flags = ImPlotLineFlags_SkipNaN;
            ImPlot::PlotLine("energy", x.data(), energies.data(), (int)energies.size(), spec);
            if (ImPlot::IsPlotHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                const ImPlotPoint mp = ImPlot::GetPlotMousePos();
                const int frame = (int)std::lround(mp.x) - 1;
                if (frame >= 0 && frame < (int)s.frames.nframes) SetFrame(state, frame);
            }
        } else {
            ImPlot::PlotText("No energies found in the xyz comment lines (e.g. `E = -76.4`)", (1 + s.frames.nframes) * 0.5, 0.5);
        }
        double current = s.activeFrame + 1;
        if (ImPlot::DragLineX(0, &current, ImVec4(1.0f, 0.85f, 0.3f, 0.9f), 1.5f, ImPlotDragToolFlags_NoFit)) {
            const int frame = std::clamp((int)std::lround(current) - 1, 0, (int)s.frames.nframes - 1);
            SetFrame(state, frame);
        }
        ImPlot::EndPlot();
    }
}

void UpdateMeasurementPlotCache(AppState& state, const Structure& s) {
    auto& cache = state.measurementPlotCache;
    if (cache.structureIndex == state.activeStructure && cache.measurementsVersion == state.measurementsVersion &&
        cache.dataVersion == s.frames.dataVersion && cache.x.size() == s.frames.nframes)
        return;
    cache.structureIndex = state.activeStructure;
    cache.measurementsVersion = state.measurementsVersion;
    cache.dataVersion = s.frames.dataVersion;
    cache.x.resize(s.frames.nframes);
    for (uint32_t i = 0; i < s.frames.nframes; ++i) cache.x[i] = i + 1;
    cache.series.assign(state.measurements.size(), {});
    for (size_t k = 0; k < state.measurements.size(); ++k) {
        cache.series[k].resize(s.frames.nframes);
        for (uint32_t i = 0; i < s.frames.nframes; ++i)
            cache.series[k][i] = MeasurementValue(s.frames.atoms[i], state.measurements[k]);
    }
}

void PlotMeasurements(AppState& state, const Structure& s) {
    UpdateMeasurementPlotCache(state, s);
    const auto& cache = state.measurementPlotCache;
    if (ImPlot::BeginPlot("##measurements", ImVec2(-1, -1), ImPlotFlags_NoMouseText)) {
        ImPlot::SetupAxes("Frame", "Value (A or deg)");
        ImPlot::SetupAxisLimits(ImAxis_X1, 0.5, std::fmax(1.5, s.frames.nframes + 0.5), ImGuiCond_Always);
        ImPlot::SetupLegend(ImPlotLocation_North, ImPlotLegendFlags_Outside | ImPlotLegendFlags_Horizontal);
        if (state.measurements.empty()) {
            ImPlot::PlotText("Click atoms in the 3D view (or `measure i j`) to plot geometry across frames", (1 + s.frames.nframes) * 0.5, 0.5);
        }
        for (size_t k = 0; k < state.measurements.size() && k < cache.series.size(); ++k) {
            ImPlotSpec spec;
            if ((int)cache.x.size() <= kMaxMarkerPoints) {
                spec.Marker = ImPlotMarker_Circle;
                spec.MarkerSize = 3.0f;
            }
            spec.Flags = ImPlotLineFlags_SkipNaN;
            ImPlot::PlotLine(MeasurementLabel(state.measurements[k]).c_str(), cache.x.data(), cache.series[k].data(),
                             (int)cache.x.size(), spec);
        }
        if (ImPlot::IsPlotHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !state.measurements.empty()) {
            const int frame = (int)std::lround(ImPlot::GetPlotMousePos().x) - 1;
            if (frame >= 0 && frame < (int)s.frames.nframes) SetFrame(state, frame);
        }
        double current = s.activeFrame + 1;
        if (ImPlot::DragLineX(0, &current, ImVec4(1.0f, 0.85f, 0.3f, 0.9f), 1.5f, ImPlotDragToolFlags_NoFit)) {
            const int frame = std::clamp((int)std::lround(current) - 1, 0, (int)s.frames.nframes - 1);
            SetFrame(state, frame);
        }
        ImPlot::EndPlot();
    }
}

}  // namespace

void DrawPlotPanel(AppState& state) {
    const Structure* s = state.ActiveStructure();
    if (!s) {
        ImGui::TextDisabled("No structure loaded.");
        ImGui::TextDisabled("Open an xyz file (File > Open, drag & drop, or `load path.xyz`).");
        return;
    }

    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const float paneWidth = ImGui::GetContentRegionAvail().x;
    if (state.twoDPlotIndex == 1) PlotMeasurements(state, *s);
    else PlotEnergy(state, *s);

    // Plot picker floated over the top-right corner of the panel (quick-mag puts
    // it top-left; here the y-axis label lives there).
    ImGui::SetNextWindowPos(ImVec2(origin.x + paneWidth - 8, origin.y + 8), ImGuiCond_Always, ImVec2(1.0f, 0.0f));
    ImGui::SetNextWindowBgAlpha(0.75f);
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                                   ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoFocusOnAppearing |
                                   ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking |
                                   ImGuiWindowFlags_AlwaysAutoResize;
    if (ImGui::Begin("##plot_picker", nullptr, flags)) {
        ImGui::SetNextItemWidth(190.0f);
        ImGui::Combo("##plot_kind", &state.twoDPlotIndex, kPlotNames, IM_ARRAYSIZE(kPlotNames));
    }
    ImGui::End();
}
