#include "ui/ui_builder.h"

#include <algorithm>
#include <filesystem>

#include <fmt/format.h>

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_stdlib.h"

#include "app/app_state.h"
#include "app/actions.h"
#include "graph/graph_system.h"
#include "ui/panel_registry.h"
#include "ui/ui.h"

namespace {

constexpr const char* kDragPayload = "CHEMLAB_PANEL";
constexpr const char* kLegacyUIsFile = "chemlab_uis.toml";

ImGuiDir ToImGuiDir(SplitDir d) {
    switch (d) {
        case SplitDir::Left: return ImGuiDir_Left;
        case SplitDir::Right: return ImGuiDir_Right;
        case SplitDir::Up: return ImGuiDir_Up;
        case SplitDir::Down: return ImGuiDir_Down;
    }
    return ImGuiDir_Left;
}

// Carve the dockspace into one node per layout slot. Returns node ids
// indexed by slot.
std::vector<ImGuiID> BuildDockNodes(const LayoutDef& layout, ImGuiID dockspaceId, ImVec2 size) {
    ImGui::DockBuilderRemoveNode(dockspaceId);
    ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspaceId, size);
    std::vector<ImGuiID> nodes{dockspaceId};
    for (const SplitOp& op : layout.splits) {
        const int parent = std::clamp(op.parent, 0, (int)nodes.size() - 1);
        ImGuiID newNode =
            ImGui::DockBuilderSplitNode(nodes[parent], ToImGuiDir(op.dir), op.fraction, nullptr, &nodes[parent]);
        nodes.push_back(newNode);
    }
    return nodes;
}

std::string SlotWindowName(int slot) { return fmt::format("Slot {}##uibuilder_slot{}", slot + 1, slot); }

const LayoutDef* DraftLayout(const UIBuilderState& b) { return FindLayout(b.draft.layoutId); }

// Which slot (if any) a panel is assigned to in `def`; -1 = none.
int AssignedSlot(const UIDefinition& def, const char* panelId) {
    for (size_t s = 0; s < def.slots.size(); ++s)
        for (const auto& ref : def.slots[s])
            if (ref.panel == panelId) return (int)s;
    return -1;
}

// ---------------------------------------------------------------------------
// Layout thumbnail: a clickable miniature of the slot arrangement.
// ---------------------------------------------------------------------------
bool LayoutThumbnail(const LayoutDef& layout, bool selected, ImVec2 size) {
    ImGui::PushID(layout.id.c_str());
    ImGui::BeginGroup();
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const bool clicked = ImGui::InvisibleButton("thumb", size);
    const bool hovered = ImGui::IsItemHovered();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImU32 fill = ImGui::GetColorU32(hovered ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg);
    const ImU32 line = ImGui::GetColorU32(selected ? ImGuiCol_CheckMark : ImGuiCol_Border);
    for (const SlotRect& r : LayoutSlotRects(layout)) {
        const ImVec2 a(pos.x + r.x * size.x + 2.0f, pos.y + r.y * size.y + 2.0f);
        const ImVec2 b(pos.x + (r.x + r.w) * size.x - 2.0f, pos.y + (r.y + r.h) * size.y - 2.0f);
        dl->AddRectFilled(a, b, fill, 2.0f);
        dl->AddRect(a, b, line, 2.0f, 0, selected ? 2.0f : 1.0f);
    }
    // Centred caption under the thumbnail.
    const ImVec2 ts = ImGui::CalcTextSize(layout.name.c_str());
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0f, (size.x - ts.x) * 0.5f));
    selected ? ImGui::Text("%s", layout.name.c_str()) : ImGui::TextDisabled("%s", layout.name.c_str());
    ImGui::EndGroup();
    ImGui::PopID();
    return clicked;
}

// ---------------------------------------------------------------------------
// Edit mode: slot placeholders + panel palette
// ---------------------------------------------------------------------------
void AssignPanelToSlot(UIDefinition& def, const char* panelId, int slot) {
    def.RemovePanel(panelId);   // a panel window can only live in one place
    if (slot >= 0 && slot < (int)def.slots.size()) def.slots[slot].push_back({panelId, true});
}

void DrawSlotPlaceholder(AppState& state, int slot) {
    UIBuilderState& b = state.uiBuilder;
    const std::string name = SlotWindowName(slot);
    if (!ImGui::Begin(name.c_str(), nullptr, ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }
    // Assigned panels, in tab order, each removable.
    auto& refs = b.draft.slots[slot];
    if (refs.empty()) {
        ImGui::TextDisabled("Empty slot");
    } else {
        for (size_t i = 0; i < refs.size();) {
            const PanelInfo* p = FindPanel(refs[i].panel);
            ImGui::PushID((int)i);
            if (ImGui::SmallButton("x")) {
                refs.erase(refs.begin() + i);
                ImGui::PopID();
                continue;
            }
            ImGui::SameLine();
            ImGui::TextUnformatted(p ? p->title : refs[i].panel.c_str());
            if (refs.size() > 1) ImGui::SetItemTooltip("Panels in one slot become tabs, top to bottom.");
            ImGui::PopID();
            ++i;
        }
    }
    ImGui::Spacing();

    // Drop zone filling the rest of the window.
    ImVec2 avail = ImGui::GetContentRegionAvail();
    avail.x = std::max(avail.x, 60.0f);
    avail.y = std::max(avail.y, 48.0f);
    ImGui::InvisibleButton("dropzone", avail);
    const ImVec2 a = ImGui::GetItemRectMin();
    const ImVec2 c = ImGui::GetItemRectMax();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const bool dragging = ImGui::GetDragDropPayload() && ImGui::GetDragDropPayload()->IsDataType(kDragPayload);
    const ImU32 line = ImGui::GetColorU32(dragging ? ImGuiCol_DragDropTarget : ImGuiCol_Border);
    dl->AddRect(a, c, line, 4.0f, 0, dragging ? 2.0f : 1.0f);
    const char* hint = "Drop a panel here";
    const ImVec2 ts = ImGui::CalcTextSize(hint);
    dl->AddText(ImVec2((a.x + c.x - ts.x) * 0.5f, (a.y + c.y - ts.y) * 0.5f), ImGui::GetColorU32(ImGuiCol_TextDisabled), hint);
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kDragPayload)) {
            const int idx = *(const int*)payload->Data;
            if (idx >= 0 && idx < (int)PanelCatalog().size()) AssignPanelToSlot(b.draft, PanelCatalog()[idx].id, slot);
        }
        ImGui::EndDragDropTarget();
    }
    ImGui::End();
}

void DrawPanelPalette(AppState& state) {
    UIBuilderState& b = state.uiBuilder;
    ImGui::SetNextWindowSize(ImVec2(260, 340), ImGuiCond_FirstUseEver);
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + vp->WorkSize.x - 290, vp->WorkPos.y + 60), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Panels##palette", nullptr, ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }
    ImGui::TextWrapped("Drag a panel onto a slot. Panels sharing a slot become tabs.");
    ImGui::Separator();
    const auto& catalog = PanelCatalog();
    for (int i = 0; i < (int)catalog.size(); ++i) {
        const PanelInfo& p = catalog[i];
        const int slot = AssignedSlot(b.draft, p.id);
        ImGui::PushID(p.id);
        ImGui::Selectable(p.title, slot >= 0);
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
            ImGui::SetDragDropPayload(kDragPayload, &i, sizeof(int));
            ImGui::Text("Place: %s", p.title);
            ImGui::EndDragDropSource();
        }
        if (ImGui::IsItemHovered()) ImGui::SetItemTooltip("%s", p.description);
        if (slot >= 0) {
            ImGui::SameLine();
            ImGui::TextDisabled("(slot %d)", slot + 1);
        }
        ImGui::PopID();
    }
    ImGui::End();
}

void DrawEditControls(AppState& state) {
    UIBuilderState& b = state.uiBuilder;
    ImGui::SetNextWindowSize(ImVec2(320, 0), ImGuiCond_FirstUseEver);
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + 20, vp->WorkPos.y + 60), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("UI Builder", nullptr, ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }
    ImGui::InputText("Name", &b.draft.name);
    const LayoutDef* layout = DraftLayout(b);
    ImGui::Text("Layout: %s", layout ? layout->name.c_str() : b.draft.layoutId.c_str());
    ImGui::Spacing();
    const bool empty = std::all_of(b.draft.slots.begin(), b.draft.slots.end(), [](const auto& s) { return s.empty(); });
    ImGui::BeginDisabled(empty || b.draft.name.empty());
    if (ImGui::Button("Save & Apply")) {
        graph::GraphSystem& gs = state.GraphSys();
        if (b.editIndex >= 0 && b.editIndex < (int)gs.scenes.size() && !gs.scenes[(size_t)b.editIndex].builtin) {
            graph::Scene& scene = gs.scenes[(size_t)b.editIndex];
            graph::Node* layout = b.editLayout ? scene.graph.FindNode(b.editLayout) : graph::ActiveLayoutNode(scene);
            if (layout && layout->typeId != graph::kLayoutNodeType) layout = nullptr;
            scene.activeLayout = graph::SetLayoutUI(scene.graph, layout, b.draft).id;
        } else {
            gs.scenes.push_back(graph::MakeScene(b.draft, false));
            b.editIndex = (int)gs.scenes.size() - 1;
        }
        graph::Scene& scene = gs.scenes[(size_t)b.editIndex];
        std::string err;
        if (!graph::SaveScene(scene, err)) LogError(state, "Could not save scene: " + err);
        b.editing = false;
        b.open = false;
        gs.SwitchScene(state, b.editIndex);   // re-dock with the real panels
    }
    ImGui::EndDisabled();
    if (empty) ImGui::SetItemTooltip("Add at least one panel first.");
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
        b.editing = false;
        b.open = true;
        state.resetLayoutRequested = true;   // restore the active UI
    }
    ImGui::End();
}

// ---------------------------------------------------------------------------
// Home mode: pick, apply, edit or create UIs
// ---------------------------------------------------------------------------
void StartEditing(AppState& state, const UIDefinition& base, int editIndex, unsigned editLayout = 0) {
    UIBuilderState& b = state.uiBuilder;
    b.draft = base;
    b.draft.builtin = false;
    b.editIndex = editIndex;
    b.editLayout = editLayout;
    if (const LayoutDef* layout = FindLayout(b.draft.layoutId)) b.draft.FitToLayout(*layout);
    b.editing = true;
    b.relayout = true;
}

void DrawBuilderHome(AppState& state) {
    UIBuilderState& b = state.uiBuilder;
    graph::GraphSystem& gs = state.GraphSys();
    ImGui::SetNextWindowSize(ImVec2(560, 480), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("UI Builder", &b.open, ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }

    ImGui::SeparatorText("Scenes");
    int deleteIndex = -1;
    for (int i = 0; i < (int)gs.scenes.size(); ++i) {
        graph::Scene& scene = gs.scenes[(size_t)i];
        const std::string name = graph::SceneName(scene);
        ImGui::PushID(i);
        if (ImGui::RadioButton(name.c_str(), gs.activeScene == i)) gs.SwitchScene(state, i);
        if (scene.builtin) {
            ImGui::SameLine();
            ImGui::TextDisabled("(built-in)");
        }
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 160);
        if (ImGui::SmallButton("Graph")) scene.graphOpen = true;
        ImGui::SameLine();
        if (ImGui::SmallButton(scene.builtin ? "Edit a copy" : "Edit")) UIBuilderEditScene(state, i);
        if (!scene.builtin) {
            ImGui::SameLine();
            if (ImGui::SmallButton("Delete")) deleteIndex = i;
        }
        // A scene with several layouts: one line per layout, indented.
        const std::vector<graph::Node*> layouts = graph::LayoutNodes(scene.graph);
        if (layouts.size() > 1) {
            const graph::Node* active = graph::ActiveLayoutNode(scene);
            for (graph::Node* n : layouts) {
                ImGui::PushID((int)n->id);
                ImGui::Indent();
                if (ImGui::RadioButton(graph::LayoutName(*n).c_str(), gs.activeScene == i && n == active)) {
                    scene.activeLayout = n->id;
                    gs.SwitchScene(state, i);
                }
                ImGui::SameLine(ImGui::GetContentRegionAvail().x - 60);
                if (ImGui::SmallButton(scene.builtin ? "Edit a copy" : "Edit")) UIBuilderEditLayout(state, i, n->id);
                ImGui::Unindent();
                ImGui::PopID();
            }
        }
        ImGui::PopID();
    }
    if (deleteIndex >= 0) {
        std::string err;
        if (!gs.RemoveScene(state, deleteIndex, err)) LogError(state, err);
    }

    ImGui::Spacing();
    ImGui::SeparatorText("New scene");
    ImGui::InputText("Name", &b.newName);
    ImGui::TextDisabled("Choose a layout:");
    const auto& layouts = BuiltinLayouts();
    const float cellW = 132.0f;
    const int perRow = std::max(1, (int)(ImGui::GetContentRegionAvail().x / cellW));
    for (int i = 0; i < (int)layouts.size(); ++i) {
        if (i % perRow != 0) ImGui::SameLine();
        if (LayoutThumbnail(layouts[i], b.newLayoutIndex == i, ImVec2(120, 78))) b.newLayoutIndex = i;
    }
    ImGui::Spacing();
    ImGui::BeginDisabled(b.newName.empty());
    if (ImGui::Button("Create and edit...")) {
        UIDefinition ui;
        ui.name = b.newName;
        ui.layoutId = layouts[b.newLayoutIndex].id;
        ui.FitToLayout(layouts[b.newLayoutIndex]);
        StartEditing(state, ui, -1);
    }
    ImGui::EndDisabled();
    if (b.newName.empty()) ImGui::SetItemTooltip("Give the scene a name first.");
    ImGui::End();
}

}  // namespace

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
void ApplyUIDockLayout(AppState& state, const UIDefinition& def, ImGuiID dockspaceId, ImVec2 size) {
    const LayoutDef* layout = FindLayout(def.layoutId);
    if (!layout) return;
    const std::vector<ImGuiID> nodes = BuildDockNodes(*layout, dockspaceId, size);
    for (size_t s = 0; s < def.slots.size() && s < nodes.size(); ++s)
        for (const UIPanelRef& ref : def.slots[s])
            if (const PanelInfo* p = FindPanel(ref.panel)) ImGui::DockBuilderDockWindow(p->title, nodes[s]);
    ImGui::DockBuilderFinish(dockspaceId);
    (void)state;
}

void ApplyUIVisibility(AppState& state, const UIDefinition& def) {
    for (const PanelInfo& p : PanelCatalog()) state.PanelOpen(p.id) = false;
    for (const auto& slot : def.slots)
        for (const UIPanelRef& ref : slot)
            if (FindPanel(ref.panel)) state.PanelOpen(ref.panel.c_str()) = ref.visible;
}

void UIBuilderPreDockspace(AppState& state, ImGuiID dockspaceId, ImVec2 size) {
    UIBuilderState& b = state.uiBuilder;
    if (!b.editing || !b.relayout) return;
    const LayoutDef* layout = DraftLayout(b);
    if (!layout) { b.editing = false; return; }
    b.draft.FitToLayout(*layout);
    const std::vector<ImGuiID> nodes = BuildDockNodes(*layout, dockspaceId, size);
    for (int s = 0; s < layout->SlotCount(); ++s) ImGui::DockBuilderDockWindow(SlotWindowName(s).c_str(), nodes[s]);
    ImGui::DockBuilderFinish(dockspaceId);
    b.relayout = false;
}

void DrawUIBuilder(AppState& state) {
    UIBuilderState& b = state.uiBuilder;
    if (b.editing) {
        for (int s = 0; s < (int)b.draft.slots.size(); ++s) DrawSlotPlaceholder(state, s);
        DrawPanelPalette(state);
        DrawEditControls(state);
    } else if (b.open) {
        DrawBuilderHome(state);
    }
}

void UIBuilderEditLayout(AppState& state, int sceneIndex, unsigned layoutNodeId) {
    graph::GraphSystem& gs = state.GraphSys();
    if (sceneIndex < 0 || sceneIndex >= (int)gs.scenes.size()) return;
    graph::Scene& scene = gs.scenes[(size_t)sceneIndex];
    graph::Node* layout = layoutNodeId ? scene.graph.FindNode(layoutNodeId) : graph::ActiveLayoutNode(scene);
    if (!layout || layout->typeId != graph::kLayoutNodeType) return;
    UIDefinition base = graph::LayoutUI(scene.graph, *layout);
    if (scene.builtin) base.name += " copy";
    StartEditing(state, base, scene.builtin ? -1 : sceneIndex, scene.builtin ? 0 : layout->id);
    state.uiBuilder.open = true;
}

void MigrateUserUIsToScenes(AppState& state) {
    if (!std::filesystem::exists(kLegacyUIsFile)) return;
    std::vector<UIDefinition> user;
    std::string activeName, error;
    if (!LoadUserUIs(kLegacyUIsFile, user, activeName, error)) {
        LogError(state, fmt::format("Could not read {}: {}", kLegacyUIsFile, error));
        return;
    }
    int migrated = 0;
    for (const UIDefinition& ui : user) {
        graph::Scene scene = graph::MakeScene(ui, false);
        std::string err;
        if (std::filesystem::exists(graph::ScenePath(scene.graphName))) continue;   // already a scene
        if (graph::SaveScene(scene, err)) ++migrated;
        else LogError(state, fmt::format("Could not migrate UI '{}' to a scene: {}", ui.name, err));
    }
    // The old "Default" / "Plot Lab" names became the "classic" / "plot-lab" scenes.
    if (activeName == "Default") activeName = "classic";
    else if (activeName == "Plot Lab") activeName = "plot-lab";
    std::string haveScene, haveLayout;
    graph::ReadActiveScene(haveScene, haveLayout);
    if (!activeName.empty() && haveScene.empty()) graph::WriteActiveScene(activeName, "");
    std::error_code ec;
    std::filesystem::rename(kLegacyUIsFile, std::string(kLegacyUIsFile) + ".migrated", ec);
    if (migrated > 0)
        LogInfo(state, fmt::format("Migrated {} user interface{} from {} into scenes/ (see `scene list`).", migrated,
                                   migrated == 1 ? "" : "s", kLegacyUIsFile));
}
