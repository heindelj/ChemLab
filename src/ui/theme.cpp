#include "ui/theme.h"

#include "imgui.h"
#include "implot.h"

#include <cstring>
#include <string>


// Embedded Roboto-Medium (Apache 2.0), generated at build time from ImGui's
// misc/fonts by binary_to_compressed_c.
extern "C" {
extern const unsigned int roboto_medium_compressed_size;
extern const unsigned int roboto_medium_compressed_data[];
}

static ImVec4 Hex(unsigned int rgb, float a = 1.0f) {
    return ImVec4(((rgb >> 16) & 0xFF) / 255.0f, ((rgb >> 8) & 0xFF) / 255.0f, (rgb & 0xFF) / 255.0f, a);
}

const char* UIThemeName(UITheme theme) {
    switch (theme) {
        case UITheme::Mocha: return "mocha";
        case UITheme::Darcula: return "darcula";
        case UITheme::Nord: return "nord";
    }
    return "?";
}

bool ParseUITheme(const char* text, UITheme& out) {
    std::string s = text;
    for (auto& ch : s) ch = (char)tolower((unsigned char)ch);
    if (s == "mocha" || s == "catppuccin") { out = UITheme::Mocha; return true; }
    if (s == "darcula") { out = UITheme::Darcula; return true; }
    if (s == "nord") { out = UITheme::Nord; return true; }
    return false;
}

void LoadChemLabFonts(float sizePixels) {
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();
    ImFontConfig cfg;
    cfg.FontDataOwnedByAtlas = false;
    // No RasterizerDensity here: rlImGui reports ImGuiBackendFlags_RendererHasTextures
    // and io.DisplayFramebufferScale, so ImGui 1.92 already bakes glyphs at the
    // backing scale. Setting it as well baked at 4x on Retina and the GPU then
    // minified the glyphs, which softened all text.
    std::strncpy(cfg.Name, "Roboto Medium", sizeof(cfg.Name) - 1);
    ImFont* font = io.Fonts->AddFontFromMemoryCompressedTTF(roboto_medium_compressed_data, (int)roboto_medium_compressed_size,
                                                           sizePixels, &cfg);
    if (font) io.FontDefault = font;
    else io.Fonts->AddFontDefault();
}

// One palette per theme; the layout code below is shared.
UIPalette ThemePalette(UITheme theme) {
    switch (theme) {
        case UITheme::Darcula:
            return {Hex(0x1e1f22), Hex(0x2b2d30), Hex(0x1a1b1e), Hex(0x3c3f41), Hex(0xbcbec4), Hex(0x7a7e85),
                    Hex(0x3574f0), Hex(0x4a86ff), Hex(0x2b62d4), Hex(0x393b40), Hex(0x43454a), Hex(0x2e436e)};
        case UITheme::Nord:
            return {Hex(0x2e3440), Hex(0x3b4252), Hex(0x272c36), Hex(0x4c566a), Hex(0xe5e9f0), Hex(0x9aa4b5),
                    Hex(0x88c0d0), Hex(0x8fbcbb), Hex(0x5e81ac), Hex(0x434c5e), Hex(0x4c566a), Hex(0x4c6a8a)};
        case UITheme::Mocha:
        default:
            // Catppuccin Mocha: warm dark greys, lavender/blue accents.
            return {Hex(0x181825), Hex(0x1e1e2e), Hex(0x11111b), Hex(0x313244), Hex(0xcdd6f4), Hex(0x9399b2),
                    Hex(0x89b4fa), Hex(0xb4befe), Hex(0x74c7ec), Hex(0x313244), Hex(0x45475a), Hex(0x3b4a75)};
    }
}

void ApplyChemLabTheme(UITheme theme) {
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* c = style.Colors;
    const UIPalette p = ThemePalette(theme);

    const ImVec4 bg = p.bg;
    const ImVec4 bgPanel = p.bgPanel;
    const ImVec4 bgInput = p.bgInput;
    const ImVec4 border = p.border;
    const ImVec4 text = p.text;
    const ImVec4 textDim = p.textDim;
    const ImVec4 accent = p.accent;
    const ImVec4 accentHover = p.accentHover;
    const ImVec4 accentActive = p.accentActive;
    const ImVec4 header = p.header;
    const ImVec4 headerHover = p.headerHover;

    c[ImGuiCol_Text] = text;
    c[ImGuiCol_TextDisabled] = textDim;
    c[ImGuiCol_WindowBg] = bgPanel;
    c[ImGuiCol_ChildBg] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_PopupBg] = ImVec4(bgPanel.x, bgPanel.y, bgPanel.z, 0.98f);
    c[ImGuiCol_Border] = border;
    c[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_FrameBg] = bgInput;
    c[ImGuiCol_FrameBgHovered] = ImVec4(bgInput.x + 0.04f, bgInput.y + 0.04f, bgInput.z + 0.04f, 1.0f);
    c[ImGuiCol_FrameBgActive] = ImVec4(bgInput.x + 0.08f, bgInput.y + 0.08f, bgInput.z + 0.08f, 1.0f);
    c[ImGuiCol_TitleBg] = bg;
    c[ImGuiCol_TitleBgActive] = bg;
    c[ImGuiCol_TitleBgCollapsed] = bg;
    c[ImGuiCol_MenuBarBg] = bg;
    c[ImGuiCol_ScrollbarBg] = bgPanel;
    c[ImGuiCol_ScrollbarGrab] = headerHover;
    c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(headerHover.x + 0.06f, headerHover.y + 0.06f, headerHover.z + 0.06f, 1.0f);
    c[ImGuiCol_ScrollbarGrabActive] = accent;
    c[ImGuiCol_CheckMark] = accentHover;
    c[ImGuiCol_SliderGrab] = accent;
    c[ImGuiCol_SliderGrabActive] = accentHover;
    c[ImGuiCol_Button] = header;
    c[ImGuiCol_ButtonHovered] = headerHover;
    c[ImGuiCol_ButtonActive] = accentActive;
    c[ImGuiCol_Header] = p.selection;
    c[ImGuiCol_HeaderHovered] = ImVec4(p.selection.x * 1.15f, p.selection.y * 1.15f, p.selection.z * 1.15f, 1.0f);
    c[ImGuiCol_HeaderActive] = ImVec4(p.selection.x * 1.3f, p.selection.y * 1.3f, p.selection.z * 1.3f, 1.0f);
    c[ImGuiCol_Separator] = border;
    c[ImGuiCol_SeparatorHovered] = accent;
    c[ImGuiCol_SeparatorActive] = accentHover;
    c[ImGuiCol_ResizeGrip] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_ResizeGripHovered] = accent;
    c[ImGuiCol_ResizeGripActive] = accentHover;
    c[ImGuiCol_Tab] = bg;
    c[ImGuiCol_TabHovered] = headerHover;
    c[ImGuiCol_TabSelected] = bgPanel;
    c[ImGuiCol_TabSelectedOverline] = accent;
    c[ImGuiCol_TabDimmed] = bg;
    c[ImGuiCol_TabDimmedSelected] = bgPanel;
    c[ImGuiCol_TabDimmedSelectedOverline] = border;
    c[ImGuiCol_DockingPreview] = ImVec4(accent.x, accent.y, accent.z, 0.6f);
    c[ImGuiCol_DockingEmptyBg] = bg;
    c[ImGuiCol_PlotLines] = accentHover;
    c[ImGuiCol_PlotLinesHovered] = Hex(0xffb86c);
    c[ImGuiCol_PlotHistogram] = accent;
    c[ImGuiCol_PlotHistogramHovered] = accentHover;
    c[ImGuiCol_TableHeaderBg] = header;
    c[ImGuiCol_TableBorderStrong] = border;
    c[ImGuiCol_TableBorderLight] = ImVec4(border.x, border.y, border.z, 0.6f);
    c[ImGuiCol_TableRowBg] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_TableRowBgAlt] = ImVec4(1, 1, 1, 0.03f);
    c[ImGuiCol_TextSelectedBg] = ImVec4(accent.x, accent.y, accent.z, 0.35f);
    c[ImGuiCol_DragDropTarget] = accentHover;
    c[ImGuiCol_NavCursor] = accentHover;
    c[ImGuiCol_NavWindowingHighlight] = ImVec4(1, 1, 1, 0.7f);
    c[ImGuiCol_NavWindowingDimBg] = ImVec4(0.8f, 0.8f, 0.8f, 0.2f);
    c[ImGuiCol_ModalWindowDimBg] = ImVec4(0, 0, 0, 0.5f);

    style.WindowRounding = 0.0f;
    style.ChildRounding = 4.0f;
    style.FrameRounding = 3.0f;
    style.PopupRounding = 4.0f;
    style.ScrollbarRounding = 6.0f;
    style.GrabRounding = 3.0f;
    style.TabRounding = 4.0f;
    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;
    style.WindowPadding = ImVec2(8, 8);
    style.FramePadding = ImVec2(7, 4);
    style.ItemSpacing = ImVec2(8, 5);
    style.ItemInnerSpacing = ImVec2(6, 4);
    style.IndentSpacing = 18.0f;
    style.ScrollbarSize = 12.0f;
    style.GrabMinSize = 10.0f;
    style.DockingSeparatorSize = 2.0f;
    style.TabBarBorderSize = 1.0f;

    if (ImPlot::GetCurrentContext()) {
        ImPlot::StyleColorsAuto();
        ImPlotStyle& ps = ImPlot::GetStyle();
        ps.Colors[ImPlotCol_PlotBg] = bgInput;
        ps.Colors[ImPlotCol_PlotBorder] = border;
        ps.Colors[ImPlotCol_FrameBg] = ImVec4(0, 0, 0, 0);
        ps.Colors[ImPlotCol_LegendBg] = ImVec4(bgPanel.x, bgPanel.y, bgPanel.z, 0.85f);
        ps.Colors[ImPlotCol_AxisGrid] = border;
        ps.Colors[ImPlotCol_AxisText] = text;
        ps.UseLocalTime = false;
    }
}
