#pragma once
#include "imgui.h"
// Colour theme for the ImGui UI, loosely modelled on HelloImGui's
// "DarculaDarker" theme that quick-mag uses.
enum class UITheme { Mocha = 0, Darcula = 1, Nord = 2 };
const char* UIThemeName(UITheme theme);
bool ParseUITheme(const char* text, UITheme& out);
void ApplyChemLabTheme(UITheme theme = UITheme::Nord);

// The colours a theme is built from, for code that draws its own chrome
// (the node editor) and wants to match the ImGui style.
struct UIPalette {
    ImVec4 bg, bgPanel, bgInput, border, text, textDim, accent, accentHover, accentActive, header, headerHover, selection;
};
UIPalette ThemePalette(UITheme theme);

// Loads the embedded UI font (Roboto) at the given size, rasterised for the
// current display scale. Call between rlImGuiBeginInitImGui/EndInitImGui.
void LoadChemLabFonts(float sizePixels);
