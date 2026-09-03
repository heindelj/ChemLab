#include "ui/ui_spec.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>

#include <fmt/format.h>
#include <toml++/toml.hpp>

// ---------------------------------------------------------------------------
// SplitDir
// ---------------------------------------------------------------------------
const char* SplitDirName(SplitDir d) {
    switch (d) {
        case SplitDir::Left: return "left";
        case SplitDir::Right: return "right";
        case SplitDir::Up: return "up";
        case SplitDir::Down: return "down";
    }
    return "left";
}

bool ParseSplitDir(const std::string& text, SplitDir& out) {
    if (text == "left") out = SplitDir::Left;
    else if (text == "right") out = SplitDir::Right;
    else if (text == "up") out = SplitDir::Up;
    else if (text == "down") out = SplitDir::Down;
    else return false;
    return true;
}

// ---------------------------------------------------------------------------
// UIDefinition helpers
// ---------------------------------------------------------------------------
bool UIDefinition::Uses(const std::string& panelId) const {
    for (const auto& slot : slots)
        for (const auto& ref : slot)
            if (ref.panel == panelId) return true;
    return false;
}

bool UIDefinition::RemovePanel(const std::string& panelId) {
    bool removed = false;
    for (auto& slot : slots) {
        auto it = std::remove_if(slot.begin(), slot.end(), [&](const UIPanelRef& r) { return r.panel == panelId; });
        if (it != slot.end()) {
            slot.erase(it, slot.end());
            removed = true;
        }
    }
    return removed;
}

// ---------------------------------------------------------------------------
// Built-in layouts
// ---------------------------------------------------------------------------
const std::vector<LayoutDef>& BuiltinLayouts() {
    static const std::vector<LayoutDef> layouts = {
        {"single", "Single", {}},
        {"two-column", "Two Column", {{0, SplitDir::Right, 0.50f}}},
        {"sidebar-left", "Sidebar Left", {{0, SplitDir::Left, 0.25f}}},
        {"sidebar-right", "Sidebar Right", {{0, SplitDir::Right, 0.30f}}},
        {"main-bottom", "Main + Bottom", {{0, SplitDir::Down, 0.30f}}},
        {"two-row", "Two Row", {{0, SplitDir::Down, 0.55f}}},
        {"three-column", "Three Column", {{0, SplitDir::Left, 0.20f}, {0, SplitDir::Right, 0.35f}}},
        {"quad", "Quad", {{0, SplitDir::Right, 0.50f}, {0, SplitDir::Down, 0.50f}, {1, SplitDir::Down, 0.50f}}},
        // The classic ChemLab arrangement:
        //   slot 0 = centre (3D view), 1 = left column, 2 = right column,
        //   3 = bottom of the left column, 4 = top of the right column,
        //   5 = bottom of the centre column (the 2D plot).
        {"chemlab-classic", "ChemLab Classic",
         {{0, SplitDir::Left, 0.20f},
          {0, SplitDir::Right, 0.40f},
          {1, SplitDir::Down, 0.30f},
          {2, SplitDir::Up, 0.32f},
          {0, SplitDir::Down, 0.30f}}},
    };
    return layouts;
}

const LayoutDef* FindLayout(const std::string& id) {
    for (const auto& l : BuiltinLayouts())
        if (l.id == id) return &l;
    return nullptr;
}

// ---------------------------------------------------------------------------
// Built-in UIs
// ---------------------------------------------------------------------------
std::vector<UIDefinition> BuiltinUIs() {
    UIDefinition def;
    def.name = "classic";
    def.layoutId = "chemlab-classic";
    def.builtin = true;
    def.slots = {
        {{"structure_view"}},                            // 0: centre
        {{"controls"}},                                  // 1: left column
        {{"calculate"}, {"output"}, {"console", false}}, // 2: right column (tabs)
        {{"export"}},                                    // 3: bottom-left
        {{"active_structure"}},                          // 4: top-right
        {{"plot_2d"}},                                   // 5: below the 3D view
    };
    // Plot Lab: the live 2D plot on top, the node graph that feeds it below.
    UIDefinition lab;
    lab.name = "plot-lab";
    lab.layoutId = "two-row";
    lab.builtin = true;
    lab.slots = {
        {{"plot_2d"}},                                   // 0: top
        {{"node_graph"}, {"graph_canvas", false}, {"console", false}},   // 1: bottom (tabs)
    };
    return {def, lab};
}

// ---------------------------------------------------------------------------
// Slot rectangles (for thumbnails and previews)
// ---------------------------------------------------------------------------
std::vector<SlotRect> LayoutSlotRects(const LayoutDef& layout) {
    std::vector<SlotRect> rects{SlotRect{0, 0, 1, 1}};
    for (const SplitOp& op : layout.splits) {
        if (op.parent < 0 || op.parent >= (int)rects.size()) { rects.push_back(rects.back()); continue; }
        SlotRect& p = rects[op.parent];
        SlotRect c = p;
        const float f = std::clamp(op.fraction, 0.05f, 0.95f);
        switch (op.dir) {
            case SplitDir::Left:
                c.w = p.w * f; p.x += c.w; p.w -= c.w; break;
            case SplitDir::Right:
                c.w = p.w * f; c.x = p.x + p.w - c.w; p.w -= c.w; break;
            case SplitDir::Up:
                c.h = p.h * f; p.y += c.h; p.h -= c.h; break;
            case SplitDir::Down:
                c.h = p.h * f; c.y = p.y + p.h - c.h; p.h -= c.h; break;
        }
        rects.push_back(c);
    }
    return rects;
}

// ---------------------------------------------------------------------------
// Persistence (reading only: chemlab_uis.toml from older builds is migrated
// into scene files by MigrateUserUIsToScenes, ui_builder.cpp)
// ---------------------------------------------------------------------------
bool ParseUIs(const std::string& text, std::vector<UIDefinition>& out, std::string& activeName, std::string& error) {
    toml::table root;
    try {
        root = toml::parse(text);
    } catch (const toml::parse_error& e) {
        error = e.what();
        return false;
    }
    out.clear();
    activeName = root["active"].value_or(std::string{});
    if (auto arr = root["ui"].as_array()) {
        for (auto& node : *arr) {
            auto t = node.as_table();
            if (!t) continue;
            UIDefinition ui;
            ui.name = (*t)["name"].value_or(std::string{"Unnamed"});
            ui.layoutId = (*t)["layout"].value_or(std::string{"single"});
            const LayoutDef* layout = FindLayout(ui.layoutId);
            const int slotCount = layout ? layout->SlotCount() : 1;
            ui.slots.resize(slotCount);
            for (int s = 0; s < slotCount; ++s) {
                if (auto panels = (*t)[fmt::format("slot{}", s)].as_array()) {
                    for (auto& p : *panels) {
                        std::string id = p.value_or(std::string{});
                        if (id.empty()) continue;
                        UIPanelRef ref;
                        if (id[0] == '~') { ref.visible = false; id.erase(0, 1); }
                        ref.panel = id;
                        ui.slots[s].push_back(ref);
                    }
                }
            }
            out.push_back(std::move(ui));
        }
    }
    return true;
}

bool LoadUserUIs(const std::string& path, std::vector<UIDefinition>& out, std::string& activeName, std::string& error) {
    std::ifstream f(path);
    if (!f) { error = fmt::format("Cannot read {}", path); return false; }
    std::stringstream ss;
    ss << f.rdbuf();
    return ParseUIs(ss.str(), out, activeName, error);
}
