#pragma once
#include "thirdparty/imgui/imgui.h"

namespace Veil {
    inline void applyTheme() {
        ImGuiStyle& s = ImGui::GetStyle();
        ImVec4* c = s.Colors;

        // geometry
        s.WindowPadding     = {12, 12};
        s.FramePadding      = {6, 4};
        s.ItemSpacing       = {8, 6};
        s.ItemInnerSpacing  = {6, 4};
        s.ScrollbarSize     = 10;
        s.GrabMinSize       = 8;
        s.WindowRounding    = 0;
        s.FrameRounding     = 2;
        s.ScrollbarRounding = 2;
        s.GrabRounding      = 2;
        s.TabRounding       = 2;
        s.WindowBorderSize  = 1;
        s.FrameBorderSize   = 1;

        // base palette
        // bg layers
        c[ImGuiCol_WindowBg]         = {0.08f, 0.08f, 0.09f, 1.00f};
        c[ImGuiCol_ChildBg]          = {0.10f, 0.10f, 0.11f, 1.00f};
        c[ImGuiCol_PopupBg]          = {0.10f, 0.10f, 0.11f, 1.00f};

        // borders
        c[ImGuiCol_Border]           = {0.22f, 0.20f, 0.28f, 1.00f};
        c[ImGuiCol_BorderShadow]     = {0.00f, 0.00f, 0.00f, 0.00f};

        // frames
        c[ImGuiCol_FrameBg]          = {0.13f, 0.12f, 0.16f, 1.00f};
        c[ImGuiCol_FrameBgHovered]   = {0.18f, 0.16f, 0.24f, 1.00f};
        c[ImGuiCol_FrameBgActive]    = {0.22f, 0.19f, 0.32f, 1.00f};

        // title
        c[ImGuiCol_TitleBg]          = {0.08f, 0.08f, 0.09f, 1.00f};
        c[ImGuiCol_TitleBgActive]    = {0.10f, 0.08f, 0.14f, 1.00f};
        c[ImGuiCol_TitleBgCollapsed] = {0.08f, 0.08f, 0.09f, 1.00f};

        // scrollbar
        c[ImGuiCol_ScrollbarBg]      = {0.08f, 0.08f, 0.09f, 1.00f};
        c[ImGuiCol_ScrollbarGrab]    = {0.28f, 0.22f, 0.42f, 1.00f};
        c[ImGuiCol_ScrollbarGrabHovered] = {0.38f, 0.30f, 0.56f, 1.00f};
        c[ImGuiCol_ScrollbarGrabActive]  = {0.52f, 0.42f, 0.72f, 1.00f};

        // accent — purple
        c[ImGuiCol_CheckMark]        = {0.72f, 0.52f, 1.00f, 1.00f};
        c[ImGuiCol_SliderGrab]       = {0.58f, 0.38f, 0.90f, 1.00f};
        c[ImGuiCol_SliderGrabActive] = {0.72f, 0.52f, 1.00f, 1.00f};

        // buttons
        c[ImGuiCol_Button]           = {0.20f, 0.16f, 0.30f, 1.00f};
        c[ImGuiCol_ButtonHovered]    = {0.38f, 0.28f, 0.60f, 1.00f};
        c[ImGuiCol_ButtonActive]     = {0.52f, 0.40f, 0.78f, 1.00f};

        // headers (selectable, collapser)
        c[ImGuiCol_Header]           = {0.22f, 0.16f, 0.36f, 1.00f};
        c[ImGuiCol_HeaderHovered]    = {0.34f, 0.26f, 0.54f, 1.00f};
        c[ImGuiCol_HeaderActive]     = {0.48f, 0.36f, 0.72f, 1.00f};

        // separator
        c[ImGuiCol_Separator]        = {0.22f, 0.20f, 0.28f, 1.00f};
        c[ImGuiCol_SeparatorHovered] = {0.52f, 0.40f, 0.78f, 1.00f};
        c[ImGuiCol_SeparatorActive]  = {0.72f, 0.52f, 1.00f, 1.00f};

        // resize
        c[ImGuiCol_ResizeGrip]       = {0.28f, 0.22f, 0.42f, 1.00f};
        c[ImGuiCol_ResizeGripHovered]= {0.52f, 0.40f, 0.78f, 1.00f};
        c[ImGuiCol_ResizeGripActive] = {0.72f, 0.52f, 1.00f, 1.00f};

        // tabs
        c[ImGuiCol_Tab]              = {0.13f, 0.12f, 0.18f, 1.00f};
        c[ImGuiCol_TabHovered]       = {0.38f, 0.28f, 0.60f, 1.00f};
        c[ImGuiCol_TabSelected]      = {0.28f, 0.20f, 0.46f, 1.00f};

        // text
        c[ImGuiCol_Text]             = {0.88f, 0.86f, 0.94f, 1.00f};
        c[ImGuiCol_TextDisabled]     = {0.36f, 0.34f, 0.44f, 1.00f};

        // plot
        c[ImGuiCol_PlotLines]        = {0.58f, 0.38f, 0.90f, 1.00f};
        c[ImGuiCol_PlotLinesHovered] = {0.72f, 0.52f, 1.00f, 1.00f};
        c[ImGuiCol_PlotHistogram]    = {0.58f, 0.38f, 0.90f, 1.00f};
        c[ImGuiCol_PlotHistogramHovered] = {0.72f, 0.52f, 1.00f, 1.00f};

        // progress bar uses PlotHistogram color automatically

        // table
        c[ImGuiCol_TableHeaderBg]    = {0.13f, 0.12f, 0.18f, 1.00f};
        c[ImGuiCol_TableBorderLight] = {0.18f, 0.16f, 0.24f, 1.00f};
        c[ImGuiCol_TableBorderStrong]= {0.22f, 0.20f, 0.28f, 1.00f};

        // nav highlight
        c[ImGuiCol_NavHighlight]     = {0.72f, 0.52f, 1.00f, 1.00f};
    }
}