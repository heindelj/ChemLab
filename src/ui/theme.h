#pragma once
// Colour theme for the ImGui UI, loosely modelled on HelloImGui's
// "DarculaDarker" theme that quick-mag uses.
enum class UITheme { Mocha = 0, Darcula = 1, Nord = 2 };
const char* UIThemeName(UITheme theme);
bool ParseUITheme(const char* text, UITheme& out);
void ApplyChemLabTheme(UITheme theme = UITheme::Nord);

// Loads the embedded UI font (Roboto) at the given size, rasterised for the
// current display scale. Call between rlImGuiBeginInitImGui/EndInitImGui.
void LoadChemLabFonts(float sizePixels);
