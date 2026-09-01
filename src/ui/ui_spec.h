#pragma once
// The UI system's data model.
//
// A *Layout* is a named recipe for carving the dockspace into slots: start
// with one slot (index 0, the whole dockspace) and apply a list of splits,
// each of which cuts a new slot off an existing one. This maps one-to-one
// onto ImGui's DockBuilderSplitNode, and is trivial to serialise and to
// preview as a thumbnail.
//
// A *UIDefinition* is a layout plus an assignment of panels (by id, see
// panel_registry.h) to slots. Several panels in one slot become tabs, in
// order. The current ChemLab arrangement is expressed as the built-in
// "Default" UI on the "chemlab-classic" layout.
//
// User-made UIs are persisted to a small TOML file (uis.toml in the project
// root, or chemlab_uis.toml next to the executable when no project is open).

#include <string>
#include <vector>

enum class SplitDir { Left, Right, Up, Down };
const char* SplitDirName(SplitDir d);
bool ParseSplitDir(const std::string& text, SplitDir& out);

struct SplitOp {
    int parent = 0;          // slot being split (keeps the remainder)
    SplitDir dir = SplitDir::Left;   // side the new slot is cut from
    float fraction = 0.5f;   // share of the parent given to the new slot
    // The new slot's index is implicit: slots are numbered in creation
    // order, so the i-th split creates slot i+1.
};

struct LayoutDef {
    std::string id;          // stable identifier, e.g. "three-column"
    std::string name;        // display name, e.g. "Three Column"
    std::vector<SplitOp> splits;
    int SlotCount() const { return (int)splits.size() + 1; }
};

// A panel placed in a UI. `visible` lets a UI dock a panel without showing
// it at startup (the classic layout keeps the Console docked but closed).
struct UIPanelRef {
    std::string panel;       // panel id from the panel registry
    bool visible = true;
};

struct UIDefinition {
    std::string name;
    std::string layoutId;
    std::vector<std::vector<UIPanelRef>> slots;   // one list per layout slot
    bool builtin = false;

    // Every slot list present and sized to the layout.
    void FitToLayout(const LayoutDef& layout) { slots.resize(layout.SlotCount()); }
    bool Uses(const std::string& panelId) const;
    // Remove `panelId` from every slot. Returns true if it was present.
    bool RemovePanel(const std::string& panelId);
};

// ---- catalogs -------------------------------------------------------------
const std::vector<LayoutDef>& BuiltinLayouts();
const LayoutDef* FindLayout(const std::string& id);
// The built-in UIs: "Default" (the classic arrangement) and "Plot Lab" (2D plot over node graph).
std::vector<UIDefinition> BuiltinUIs();

// ---- geometry helper (thumbnails / previews) ------------------------------
struct SlotRect { float x = 0, y = 0, w = 1, h = 1; };   // in [0,1] space
std::vector<SlotRect> LayoutSlotRects(const LayoutDef& layout);

// ---- persistence of user (non-builtin) UIs --------------------------------
// The file also records which UI was active (`active = "..."`), so that on
// the next launch the restored dock layout (ImGui's ini file) is interpreted
// against the same UI it was saved under.
std::string SerialiseUIs(const std::vector<UIDefinition>& uis, const std::string& activeName);
bool ParseUIs(const std::string& text, std::vector<UIDefinition>& out, std::string& activeName, std::string& error);
bool SaveUserUIs(const std::string& path, const std::vector<UIDefinition>& uis, const std::string& activeName,
                 std::string& error);
bool LoadUserUIs(const std::string& path, std::vector<UIDefinition>& out, std::string& activeName, std::string& error);

// ---- UI builder (interactive editor) state --------------------------------
struct UIBuilderState {
    bool open = false;        // the "UI Builder" window is shown
    bool editing = false;     // drag-and-drop edit mode is active
    UIDefinition draft;       // the UI being edited
    int editIndex = -1;       // index into AppState::uis, -1 = brand new
    std::string newName;      // "New UI" form fields
    int newLayoutIndex = 0;
    bool relayout = false;    // dock nodes must be rebuilt for the draft
};
