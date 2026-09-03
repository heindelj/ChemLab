// Structure View: the raylib 3D viewport shown as an image, with the camera
// toolbar and the measurement / selection overlays. The 2D plot that used to
// share this panel now lives in its own dockable panel (panel_plot.cpp).

#include <algorithm>
#include <cmath>

#include <fmt/format.h>

#include "imgui.h"
#include "raylib.h"

#include "app/actions.h"
#include "core/math_utils.h"
#include "ui/ui.h"

namespace {

constexpr float kClickDragThreshold = 4.0f;   // pixels of motion that turn a click into a drag

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
    const Atoms* atoms = state.ActiveAtoms();

    DrawToolbar(state);

    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const ImVec2 size(std::fmax(avail.x, 1.0f), std::fmax(avail.y, 1.0f));
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
        const std::string msg = "Open an xyz file (File > Open, drag & drop, or `load path.xyz`)";
        const ImVec2 ts = ImGui::CalcTextSize(msg.c_str());
        dl->AddText(ImVec2(gView.imageMin.x + (size.x - ts.x) * 0.5f, gView.imageMin.y + (size.y - ts.y) * 0.5f), IM_COL32(180, 180, 180, 255), msg.c_str());
    }
}
