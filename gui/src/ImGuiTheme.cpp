#include "ImGuiTheme.hpp"

#include <imgui.h>

namespace Gui
{

    void applyChessTheme()
    {
        ImGuiStyle& style = ImGui::GetStyle();
        ImVec4* colors = style.Colors;

        // Geometry: rounded corners, comfortable padding
        style.WindowRounding = 6.0f;
        style.ChildRounding = 5.0f;
        style.FrameRounding = 4.0f;
        style.PopupRounding = 4.0f;
        style.ScrollbarRounding = 4.0f;
        style.GrabRounding = 4.0f;

        style.WindowPadding = ImVec2(12.0f, 12.0f);
        style.FramePadding = ImVec2(9.0f, 5.0f);
        style.ItemSpacing = ImVec2(8.0f, 7.0f);
        style.ItemInnerSpacing = ImVec2(6.0f, 5.0f);
        style.ScrollbarSize = 12.0f;
        style.GrabMinSize = 10.0f;

        style.WindowBorderSize = 1.0f;
        style.ChildBorderSize = 1.0f;
        style.FrameBorderSize = 0.0f;
        style.PopupBorderSize = 1.0f;

        // Rounded fills read smoother with the fill AA on and the
        // texture-based line AA off (the latter can shimmer at 1x DPI).
        style.AntiAliasedLines = true;
        style.AntiAliasedLinesUseTex = false;
        style.AntiAliasedFill = true;

        // Palette
        // Deep slate background, warm off-white text, amber accent.
        // Deliberately low-saturation so the board's colors dominate.
        const ImVec4 bgDeep = ImVec4(0.100f, 0.098f, 0.096f, 1.00f);
        const ImVec4 bgBase = ImVec4(0.140f, 0.138f, 0.136f, 1.00f);
        const ImVec4 bgRaised = ImVec4(0.190f, 0.188f, 0.186f, 1.00f);
        const ImVec4 bgHover = ImVec4(0.250f, 0.248f, 0.246f, 1.00f);
        const ImVec4 accent = ImVec4(0.91f, 0.78f, 0.48f, 1.00f);
        const ImVec4 accentDim = ImVec4(0.91f, 0.78f, 0.48f, 0.55f);
        const ImVec4 text = ImVec4(0.930f, 0.928f, 0.926f, 1.00f);
        const ImVec4 textDim = ImVec4(0.640f, 0.638f, 0.636f, 1.00f);

        colors[ImGuiCol_Text] = text;
        colors[ImGuiCol_TextDisabled] = textDim;
        colors[ImGuiCol_WindowBg] = bgBase;
        colors[ImGuiCol_ChildBg] = bgDeep;
        colors[ImGuiCol_PopupBg] = bgRaised;
        colors[ImGuiCol_Border] = ImVec4(0.290f, 0.288f, 0.286f, 0.80f);
        colors[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);

        colors[ImGuiCol_FrameBg] = bgRaised;
        colors[ImGuiCol_FrameBgHovered] = bgHover;
        colors[ImGuiCol_FrameBgActive] = bgHover;

        colors[ImGuiCol_TitleBg] = bgDeep;
        colors[ImGuiCol_TitleBgActive] = bgRaised;
        colors[ImGuiCol_TitleBgCollapsed] = bgDeep;
        colors[ImGuiCol_MenuBarBg] = bgDeep;

        colors[ImGuiCol_ScrollbarBg] = bgDeep;
        colors[ImGuiCol_ScrollbarGrab] = bgRaised;
        colors[ImGuiCol_ScrollbarGrabHovered] = bgHover;
        colors[ImGuiCol_ScrollbarGrabActive] = accentDim;

        colors[ImGuiCol_CheckMark] = accent;
        colors[ImGuiCol_SliderGrab] = accent;
        colors[ImGuiCol_SliderGrabActive] = accent;

        colors[ImGuiCol_Button] = bgRaised;
        colors[ImGuiCol_ButtonHovered] = bgHover;
        colors[ImGuiCol_ButtonActive] = accentDim;

        colors[ImGuiCol_Header] = bgRaised;
        colors[ImGuiCol_HeaderHovered] = bgHover;
        colors[ImGuiCol_HeaderActive] = accentDim;

        colors[ImGuiCol_Separator] = ImVec4(0.290f, 0.288f, 0.286f, 0.70f);
        colors[ImGuiCol_SeparatorHovered] = accentDim;
        colors[ImGuiCol_SeparatorActive] = accent;

        colors[ImGuiCol_ResizeGrip] = bgRaised;
        colors[ImGuiCol_ResizeGripHovered] = bgHover;
        colors[ImGuiCol_ResizeGripActive] = accentDim;

        colors[ImGuiCol_DockingPreview] = accentDim;
        colors[ImGuiCol_DockingEmptyBg] = bgDeep;

        colors[ImGuiCol_TableHeaderBg] = bgRaised;
        colors[ImGuiCol_TableBorderStrong] = ImVec4(0.290f, 0.288f, 0.286f, 1.00f);
        colors[ImGuiCol_TableBorderLight] = ImVec4(0.220f, 0.218f, 0.216f, 1.00f);
        colors[ImGuiCol_TableRowBg] = ImVec4(0, 0, 0, 0);
        colors[ImGuiCol_TableRowBgAlt] = ImVec4(1, 1, 1, 0.02f);

        colors[ImGuiCol_TextSelectedBg] = accentDim;
    }

} // namespace Gui