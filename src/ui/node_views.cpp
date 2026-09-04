// The windows owned by visualize nodes. A Render 3D node dropped into a
// canvas (rather than living in the Structure View's own graph) gets a
// floating "3D: <node>" window with its own render texture, camera and GPU
// model, showing whatever ChemicalData reached the node; a Plot 2D node gets
// a "Plot: <node>" window. The windows are ordinary docking windows, so they
// can be docked next to (or inside) the canvas, torn off, or closed and
// reopened from the node body. Deleting the node removes its window.
//
// The graph side only fills GraphSystem::nodeViews (atoms / plot spec +
// a version); everything GPU-related lives here, keyed by the node's uid.
// Each 3D window renders its own atoms with its own camera: the main
// Structure View keeps drawing the active structure, and mouse input only
// goes to the window the pointer is over.

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <memory>
#include <set>

#include <fmt/format.h>

#include "imgui.h"
#include "raylib.h"
#include "raymath.h"

#include "app/app_state.h"
#include "core/math_utils.h"
#include "graph/graph_system.h"
#include "render/molecular_model.h"
#include "render/viewport.h"
#include "ui/ui.h"

namespace {

constexpr float kClickDragThreshold = 4.0f;

// The GPU side of one 3D node window.
struct View3DInstance {
    Viewport viewport;
    MolecularModel model;
    uint64_t builtVersion = ~0ull;   // NodeView::version the model was built from
    bool fitted = false;             // camera framed the atoms at least once
    ImVec2 leftPressPos;
    bool leftDragging = false;
};

std::map<uint64_t, std::unique_ptr<View3DInstance>> gInstances;

void FrameAtoms(OrbitCamera& orbit, const ChemicalData& atoms) {
    if (atoms.natoms == 0) {
        orbit.Reset(Vector3Zero(), 20.0f);
        return;
    }
    Vector3 lo{std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max()};
    Vector3 hi{-lo.x, -lo.y, -lo.z};
    for (uint32_t i = 0; i < atoms.natoms; ++i) {
        const Vector3 p = AtomPos(atoms, i);
        lo = Vector3Min(lo, p);
        hi = Vector3Max(hi, p);
    }
    orbit.FrameBounds(lo - Vector3{1, 1, 1}, hi + Vector3{1, 1, 1});
}

void DrawLabel(ImDrawList* dl, ImVec2 pos, const std::string& text, ImU32 color) {
    const ImVec2 size = ImGui::CalcTextSize(text.c_str());
    dl->AddRectFilled(ImVec2(pos.x - 3, pos.y - 2), ImVec2(pos.x + size.x + 3, pos.y + size.y + 2), IM_COL32(20, 20, 20, 170), 3.0f);
    dl->AddText(pos, color, text.c_str());
}

void Draw3DWindowContents(AppState& state, graph::NodeView& view, View3DInstance& inst) {
    inst.viewport.Init();
    if (inst.builtVersion != view.version) {
        // Keep per-atom colours across frames of the same structure, as the
        // main view does.
        if (inst.model.IsLoaded() && inst.model.AtomCount() == view.atoms.natoms)
            inst.model.UpdateGeometry(view.atoms, state.render);
        else
            inst.model.Build(view.atoms, state.render);
        inst.builtVersion = view.version;
        if (!inst.fitted) {
            FrameAtoms(inst.viewport.orbit, view.atoms);
            inst.fitted = true;
        }
    }

    // ---- toolbar ----
    if (ImGui::SmallButton("Fit")) FrameAtoms(inst.viewport.orbit, view.atoms);
    ImGui::SameLine();
    const char* axes[] = {"x", "y", "z"};
    for (int axis = 0; axis < 3; ++axis) {
        static bool flipped[3] = {false, false, false};
        if (ImGui::SmallButton(axes[axis])) {
            inst.viewport.orbit.LookDownAxis(axis, flipped[axis]);
            flipped[axis] = !flipped[axis];
        }
        ImGui::SameLine();
    }
    ImGui::TextDisabled("%s  |  %u atoms", view.label.c_str(), view.atoms.natoms);

    // ---- viewport image ----
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const ImVec2 size(std::fmax(avail.x, 1.0f), std::fmax(avail.y, 1.0f));
    const float dpi = std::fmax(GetWindowScaleDPI().x, 1.0f);
    inst.viewport.Resize((int)(size.x * dpi), (int)(size.y * dpi));

    static const std::set<int> kNoHighlight;
    ViewportScene scene;
    scene.model = &inst.model;
    scene.highlighted = &kNoHighlight;
    scene.settings = &state.render;
    scene.drawGrid = state.drawGrid;
    scene.background = state.background;
    inst.viewport.Render(scene);

    const ImVec2 imageMin = ImGui::GetCursorScreenPos();
    const ImVec2 imageMax(imageMin.x + size.x, imageMin.y + size.y);
    ImGui::GetWindowDrawList()->AddImage((ImTextureID)(intptr_t)inst.viewport.Target().texture.id, imageMin, imageMax,
                                         ImVec2(0, 1), ImVec2(1, 0));
    ImGui::InvisibleButton("##nodeview3d", size,
                           ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_MouseButtonMiddle);

    // ---- camera input: only while the pointer is over this window's image ----
    ImGuiIO& io = ImGui::GetIO();
    OrbitCamera& orbit = inst.viewport.orbit;
    const bool hovered = ImGui::IsItemHovered();
    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        inst.leftPressPos = ImGui::GetMousePos();
        inst.leftDragging = false;
    }
    if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && ImGui::IsItemActive()) {
        const ImVec2 m = ImGui::GetMousePos();
        if (std::hypot(m.x - inst.leftPressPos.x, m.y - inst.leftPressPos.y) > kClickDragThreshold) inst.leftDragging = true;
        if (inst.leftDragging) orbit.Rotate(Vector2{io.MouseDelta.x, io.MouseDelta.y});
    }
    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) inst.leftDragging = false;
    if (ImGui::IsItemActive() && (ImGui::IsMouseDragging(ImGuiMouseButton_Right) || ImGui::IsMouseDragging(ImGuiMouseButton_Middle)))
        orbit.Pan(Vector2{io.MouseDelta.x, io.MouseDelta.y});
    if (hovered && io.MouseWheel != 0.0f) orbit.Zoom(io.MouseWheel);
    if (hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Right)) FrameAtoms(orbit, view.atoms);

    // Hover pick: name the atom under the pointer.
    if (hovered && !inst.leftDragging && inst.model.IsLoaded()) {
        const ImVec2 m = ImGui::GetMousePos();
        const Vector2 vp{(m.x - imageMin.x) * dpi, (m.y - imageMin.y) * dpi};
        const int hit = inst.model.PickAtom(inst.viewport.ViewportRay(vp));
        if (hit >= 0 && hit < (int)view.atoms.natoms) {
            const Vector3 p = AtomPos(view.atoms, (size_t)hit);
            ImGui::SetTooltip("%d. %s  (%.3f, %.3f, %.3f)", hit + 1, view.atoms.Label((size_t)hit).c_str(), p.x, p.y, p.z);
        }
    }

    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->PushClipRect(imageMin, imageMax, true);
    if (state.drawAtomNumbers) {
        for (uint32_t i = 0; i < view.atoms.natoms; ++i) {
            const Vector2 v = inst.viewport.WorldToViewport(AtomPos(view.atoms, i));
            dl->AddText(ImVec2(imageMin.x + v.x / dpi + 4, imageMin.y + v.y / dpi - 6), IM_COL32(140, 255, 140, 255),
                        std::to_string(i + 1).c_str());
        }
    }
    DrawLabel(dl, ImVec2(imageMin.x + 8, imageMin.y + 8), view.label, IM_COL32(220, 220, 220, 230));
    dl->PopClipRect();
}

}  // namespace

void DrawNodeViewWindows(AppState& state) {
    graph::GraphSystem& gs = state.GraphSys();
    gs.PruneViews();
    // Release the GPU side of windows whose node is gone.
    for (auto it = gInstances.begin(); it != gInstances.end();)
        it = gs.nodeViews.count(it->first) ? std::next(it) : gInstances.erase(it);

    int cascade = 0;
    for (auto& [uid, view] : gs.nodeViews) {
        if (!view.open) continue;
        const bool is3D = view.kind == graph::NodeViewKind::View3D;
        const std::string title =
            fmt::format("{}: {}###nodeview_{}", is3D ? "3D" : "Plot", view.title, uid);
        const ImVec2 origin = ImGui::GetMainViewport()->WorkPos;
        ImGui::SetNextWindowPos(ImVec2(origin.x + 120.0f + 30.0f * (float)cascade, origin.y + 90.0f + 30.0f * (float)cascade),
                                ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(520, 400), ImGuiCond_FirstUseEver);
        ++cascade;
        if (is3D) ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 4));
        if (ImGui::Begin(title.c_str(), &view.open, ImGuiWindowFlags_NoCollapse)) {
            if (is3D) {
                std::unique_ptr<View3DInstance>& inst = gInstances[uid];
                if (!inst) inst = std::make_unique<View3DInstance>();
                Draw3DWindowContents(state, view, *inst);
            } else {
                DrawNamedPlot(view.plot);
            }
        }
        ImGui::End();
        if (is3D) ImGui::PopStyleVar();
    }
}

void NodeViewsShutdown() { gInstances.clear(); }
