// Structure View: the raylib 3D viewport shown as an image, with a draggable
// splitter and an ImPlot pane beneath it (mirrors quick-mag's structure view).

#include <algorithm>
#include <cmath>
#include <limits>

#include <fmt/format.h>

#include "imgui.h"
#include "implot.h"
#include "raylib.h"

#include "app/actions.h"
#include "core/math_utils.h"
#include "ui/ui.h"

namespace {

constexpr float kSplitterThickness = 8.0f;
constexpr float kMinPlot3DHeight = 120.0f;
constexpr float kMinPlot2DHeight = 80.0f;
constexpr float kClickDragThreshold = 4.0f;   // pixels of motion that turn a click into a drag

const char* kPlotNames[] = {"Energy per frame", "Measurements per frame"};

struct ViewportInteraction {
    ImVec2 imageMin, imageMax;   // screen rect of the image
    float pixelScale = 1.0f;     // render-texture pixels per ImGui unit
    ImVec2 leftPressPos;
    bool leftDragging = false;
};
ViewportInteraction gView;

ImVec2 ToScreen(const AppState& state, Vector3 world) {
    const Vector2 v = state.viewport.WorldToViewport(world);
    return ImVec2(gView.imageMin.x + v.x / gView.pixelScale, gView.imageMin.y + v.y / gView.pixelScale);
}

Vector2 MouseInViewport() {
    const ImVec2 m = ImGui::GetMousePos();
    return Vector2{(m.x - gView.imageMin.x) * gView.pixelScale, (m.y - gView.imageMin.y) * gView.pixelScale};
}

void DrawDashedLine(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 color, float thickness, float dash = 6.0f) {
    const ImVec2 d(b.x - a.x, b.y - a.y);
    const float len = std::sqrt(d.x * d.x + d.y * d.y);
    if (len < 1e-3f) return;
    const ImVec2 u(d.x / len, d.y / len);
    for (float t = 0.0f; t < len; t += 2.0f * dash) {
        const float t2 = std::fmin(t + dash, len);
        dl->AddLine(ImVec2(a.x + u.x * t, a.y + u.y * t), ImVec2(a.x + u.x * t2, a.y + u.y * t2), color, thickness);
    }
}

void DrawLabel(ImDrawList* dl, ImVec2 pos, const std::string& text, ImU32 color) {
    const ImVec2 size = ImGui::CalcTextSize(text.c_str());
    dl->AddRectFilled(ImVec2(pos.x - 3, pos.y - 2), ImVec2(pos.x + size.x + 3, pos.y + size.y + 2), IM_COL32(20, 20, 20, 170), 3.0f);
    dl->AddText(pos, color, text.c_str());
}

void DrawMeasurementOverlay(ImDrawList* dl, const AppState& state, const Atoms& atoms, const int* idx, int count, ImU32 color,
                            bool toCursor) {
    std::vector<ImVec2> pts;
    for (int i = 0; i < count; ++i) pts.push_back(ToScreen(state, atoms.xyz[idx[i]]));
    for (size_t i = 0; i + 1 < pts.size(); ++i) DrawDashedLine(dl, pts[i], pts[i + 1], color, 2.0f);
    if (toCursor && !pts.empty()) DrawDashedLine(dl, pts.back(), ImGui::GetMousePos(), color, 1.5f);
    if (!state.drawMeasurements) return;
    Measurement m;
    m.count = count;
    for (int i = 0; i < count; ++i) m.atoms[i] = idx[i];
    if (count >= 2) {
        const double v = MeasurementValue(atoms, m);
        const std::string text = count == 2 ? fmt::format("{:.3f} A", v) : fmt::format("{:.2f}°", v);
        ImVec2 at = count == 2 ? ImVec2(0.5f * (pts[0].x + pts[1].x), 0.5f * (pts[0].y + pts[1].y))
                  : count == 3 ? ImVec2(0.5f * (pts[0].x + pts[2].x), 0.5f * (pts[0].y + pts[2].y))
                               : ImVec2(0.5f * (pts[1].x + pts[2].x), 0.5f * (pts[1].y + pts[2].y) - 14.0f);
        DrawLabel(dl, at, text, color);
    }
}

void DrawOverlays(AppState& state, const Atoms& atoms) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->PushClipRect(gView.imageMin, gView.imageMax, true);

    const ImU32 committed = IM_COL32(120, 230, 120, 255);
    const ImU32 pending = IM_COL32(255, 220, 80, 255);
    for (const Measurement& m : state.measurements)
        DrawMeasurementOverlay(dl, state, atoms, m.atoms.data(), m.count, committed, false);
    if (state.pendingCount > 0)
        DrawMeasurementOverlay(dl, state, atoms, state.pendingMeasurement.data(), state.pendingCount, pending, true);

    if (state.drawAtomNumbers) {
        for (uint32_t i = 0; i < atoms.natoms; ++i) {
            const ImVec2 p = ToScreen(state, atoms.xyz[i]);
            dl->AddText(ImVec2(p.x + 4, p.y - 6), IM_COL32(140, 255, 140, 255), std::to_string(i + 1).c_str());
        }
    }

    // Small frame/structure badge in the top-left of the view.
    const Structure* s = state.ActiveStructure();
    if (s) {
        const std::string badge = s->frames.nframes > 1
            ? fmt::format("{}  |  frame {}/{}  |  {} atoms", s->name, s->activeFrame + 1, s->frames.nframes, atoms.natoms)
            : fmt::format("{}  |  {} atoms", s->name, atoms.natoms);
        DrawLabel(dl, ImVec2(gView.imageMin.x + 8, gView.imageMin.y + 8), badge, IM_COL32(220, 220, 220, 230));
        if (state.pendingCount > 0) {
            const char* hint = state.pendingCount == 1 ? "click a 2nd atom for a distance"
                             : state.pendingCount == 2 ? "click a 3rd atom for an angle, or Enter to keep the distance"
                                                       : "click a 4th atom for a dihedral, or Enter to keep the angle";
            DrawLabel(dl, ImVec2(gView.imageMin.x + 8, gView.imageMin.y + 30), hint, pending);
        }
    }
    dl->PopClipRect();
}

void HandleViewportInput(AppState& state, const Atoms& atoms) {
    ImGuiIO& io = ImGui::GetIO();
    const bool hovered = ImGui::IsItemHovered();
    OrbitCamera& orbit = state.viewport.orbit;

    // Hover pick (cheap: analytic sphere test)
    state.hoveredAtom = -1;
    if (hovered && !gView.leftDragging) {
        state.hoveredAtom = state.model.PickAtom(state.viewport.ViewportRay(MouseInViewport()));
        if (state.hoveredAtom >= 0) {
            const Vector3& p = atoms.xyz[state.hoveredAtom];
            ImGui::SetTooltip("%d. %s  (%.3f, %.3f, %.3f)", state.hoveredAtom + 1, atoms.labels[state.hoveredAtom].c_str(), p.x, p.y, p.z);
        }
    }

    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        gView.leftPressPos = ImGui::GetMousePos();
        gView.leftDragging = false;
    }
    if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && (hovered || gView.leftDragging) && ImGui::IsItemActive()) {
        const ImVec2 m = ImGui::GetMousePos();
        const float moved = std::hypot(m.x - gView.leftPressPos.x, m.y - gView.leftPressPos.y);
        if (moved > kClickDragThreshold) gView.leftDragging = true;
        if (gView.leftDragging) orbit.Rotate(Vector2{io.MouseDelta.x, io.MouseDelta.y});
    }
    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) && ImGui::IsItemHovered()) {
        if (!gView.leftDragging) {
            const int hit = state.model.PickAtom(state.viewport.ViewportRay(MouseInViewport()));
            if (io.KeyShift) {
                if (hit >= 0) ToggleAtomSelected(state, hit);
            } else if (hit >= 0) {
                MeasurementClick(state, hit);
            } else {
                CancelPendingMeasurement(state);
                state.selected.clear();
            }
        }
        gView.leftDragging = false;
    }
    if (ImGui::IsItemActive() && (ImGui::IsMouseDragging(ImGuiMouseButton_Right) || ImGui::IsMouseDragging(ImGuiMouseButton_Middle)))
        orbit.Pan(Vector2{io.MouseDelta.x, io.MouseDelta.y});
    if (hovered && io.MouseWheel != 0.0f) orbit.Zoom(io.MouseWheel);
    if (hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Right)) ResetCamera(state);
}

float DrawPaneSplitter(const char* id, float twoDFraction, float available) {
    // Same idea as quick-mag's draw_pane_splitter: an invisible button drawn
    // as a rule with a grip, dragging it changes the split fraction.
    ImGui::Spacing();
    const ImVec2 top = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetContentRegionAvail().x;
    ImGui::InvisibleButton(id, ImVec2(std::fmax(width, 1.0f), kSplitterThickness));
    const bool hovered = ImGui::IsItemHovered(), active = ImGui::IsItemActive();
    if (hovered || active) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
    if (active) {
        const float usable = std::fmax(available - kSplitterThickness, 1.0f);
        twoDFraction -= ImGui::GetIO().MouseDelta.y / usable;
    }
    const ImU32 color = ImGui::GetColorU32(active ? ImGuiCol_SeparatorActive : hovered ? ImGuiCol_SeparatorHovered : ImGuiCol_Separator);
    const float middle = top.y + kSplitterThickness * 0.5f;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddLine(ImVec2(top.x, middle), ImVec2(top.x + width, middle), color, (hovered || active) ? 2.0f : 1.0f);
    const float grip = std::fmin(40.0f, width);
    const float centre = top.x + width * 0.5f;
    for (float off : {-2.0f, 2.0f})
        dl->AddLine(ImVec2(centre - grip * 0.5f, middle + off), ImVec2(centre + grip * 0.5f, middle + off), color, 1.0f);
    return std::clamp(twoDFraction, 0.0f, 0.9f);
}

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

void DrawTwoDPane(AppState& state, const Structure& s) {
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const float paneWidth = ImGui::GetContentRegionAvail().x;
    if (state.twoDPlotIndex == 1) PlotMeasurements(state, s);
    else PlotEnergy(state, s);
    // Plot picker floated over the top-right corner of the pane (quick-mag puts
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

void DrawToolbar(AppState& state) {
    ImGui::AlignTextToFramePadding();
    ImGui::TextDisabled("Look down:");
    for (int axis = 0; axis < 3; ++axis) {
        ImGui::SameLine();
        static bool flipped[3] = {false, false, false};
        const char* names[] = {"x", "y", "z"};
        if (ImGui::SmallButton(names[axis])) {
            LookDownAxis(state, axis, flipped[axis]);
            flipped[axis] = !flipped[axis];
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Look down the %s axis. Click again for the opposite face.", names[axis]);
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("-5°")) state.viewport.orbit.RotateAroundUp(-5.0f);
    ImGui::SameLine();
    if (ImGui::SmallButton("+5°")) state.viewport.orbit.RotateAroundUp(5.0f);
    ImGui::SameLine();
    if (ImGui::SmallButton("Fit")) ResetCamera(state);
    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();
    int style = (int)state.render.style;
    ImGui::SetNextItemWidth(130.0f);
    if (ImGui::Combo("##style", &style, "ball-and-stick\0spheres\0sticks\0")) {
        state.render.style = (RenderStyle)style;
        MarkGeometryChanged(state);
    }
    ImGui::SameLine();
    ImGui::Checkbox("Grid", &state.drawGrid);
    ImGui::SameLine();
    ImGui::Checkbox("Numbers", &state.drawAtomNumbers);

    // Right-aligned hint
    const char* hint = "drag: rotate   right-drag: pan   wheel: zoom   click: measure   shift-click: select";
    const float w = ImGui::CalcTextSize(hint).x;
    ImGui::SameLine();
    if (ImGui::GetContentRegionAvail().x > w + 20.0f) {
        ImGui::SameLine(ImGui::GetWindowWidth() - w - 12.0f);
        ImGui::TextDisabled("%s", hint);
    } else {
        ImGui::NewLine();
    }
}

}  // namespace

void DrawStructureViewPanel(AppState& state) {
    if (state.modelDirty) RebuildModel(state);
    const Structure* s = state.ActiveStructure();
    const Atoms* atoms = state.ActiveAtoms();

    DrawToolbar(state);

    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const bool showTwoD = s && avail.y > kMinPlot3DHeight + kMinPlot2DHeight + kSplitterThickness;
    float height3D = avail.y;
    float height2D = 0.0f;
    if (showTwoD) {
        const float usable = avail.y - kSplitterThickness;
        height2D = std::clamp(usable * state.twoDPaneFraction, kMinPlot2DHeight, usable - kMinPlot3DHeight);
        height3D = usable - height2D;
    }

    // ---- 3D view ----
    const ImVec2 size(std::fmax(avail.x, 1.0f), std::fmax(height3D, 1.0f));
    const float dpi = std::fmax(GetWindowScaleDPI().x, 1.0f);
    gView.pixelScale = dpi;
    state.viewport.Resize((int)(size.x * dpi), (int)(size.y * dpi));

    if (state.autoRotate) state.viewport.orbit.RotateAroundUp(state.autoRotateDegPerSec * ImGui::GetIO().DeltaTime);

    ViewportScene scene;
    scene.model = &state.model;
    scene.highlighted = &state.selected;
    scene.settings = &state.render;
    scene.drawGrid = state.drawGrid;
    scene.background = state.background;
    state.viewport.Render(scene);

    gView.imageMin = ImGui::GetCursorScreenPos();
    gView.imageMax = ImVec2(gView.imageMin.x + size.x, gView.imageMin.y + size.y);
    // ImageButton-like behaviour without the frame: an invisible button gives us
    // hover/active state, the image is drawn underneath.
    ImGui::GetWindowDrawList()->AddImage((ImTextureID)(intptr_t)state.viewport.Target().texture.id, gView.imageMin, gView.imageMax,
                                         ImVec2(0, 1), ImVec2(1, 0));
    ImGui::InvisibleButton("##viewport", size, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_MouseButtonMiddle);
    if (atoms) {
        HandleViewportInput(state, *atoms);
        DrawOverlays(state, *atoms);
    } else {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const char* msg = "Open an xyz file (File > Open, drag & drop, or `load path.xyz`)";
        const ImVec2 ts = ImGui::CalcTextSize(msg);
        dl->AddText(ImVec2(gView.imageMin.x + (size.x - ts.x) * 0.5f, gView.imageMin.y + (size.y - ts.y) * 0.5f), IM_COL32(180, 180, 180, 255), msg);
    }

    // ---- splitter + 2D pane ----
    if (showTwoD) {
        state.twoDPaneFraction = DrawPaneSplitter("##structure_pane_splitter", state.twoDPaneFraction, avail.y);
        DrawTwoDPane(state, *s);
    }
}
