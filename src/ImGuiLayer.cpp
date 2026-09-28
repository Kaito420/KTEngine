#include "ImGuiLayer.h"
#include "imgui.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx12.h"
#include "Renderer.h"
#include <d3d12.h>

#include <cstdio>

static EditorTheme s_currentTheme = EditorTheme::ModernEngine;

static void ApplyModernEngineTheme() {
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    style.WindowRounding    = 6.0f;
    style.ChildRounding     = 4.0f;
    style.FrameRounding     = 4.0f;
    style.PopupRounding     = 5.0f;
    style.ScrollbarRounding = 9.0f;
    style.GrabRounding      = 3.0f;
    style.TabRounding       = 4.0f;

    style.WindowBorderSize  = 1.0f;
    style.FrameBorderSize   = 1.0f;
    style.PopupBorderSize   = 1.0f;

    style.WindowPadding     = ImVec2(10.0f, 10.0f);
    style.FramePadding      = ImVec2(8.0f, 5.0f);
    style.ItemSpacing       = ImVec2(8.0f, 6.0f);
    style.ItemInnerSpacing  = ImVec2(6.0f, 4.0f);
    style.ScrollbarSize     = 14.0f;
    style.GrabMinSize       = 12.0f;

    colors[ImGuiCol_Text]                  = ImVec4(0.92f, 0.93f, 0.95f, 1.00f);
    colors[ImGuiCol_TextDisabled]          = ImVec4(0.48f, 0.50f, 0.55f, 1.00f);
    colors[ImGuiCol_WindowBg]              = ImVec4(0.12f, 0.12f, 0.14f, 1.00f);
    colors[ImGuiCol_ChildBg]               = ImVec4(0.14f, 0.14f, 0.16f, 0.00f);
    colors[ImGuiCol_PopupBg]               = ImVec4(0.14f, 0.14f, 0.16f, 0.98f);
    colors[ImGuiCol_Border]                = ImVec4(0.24f, 0.25f, 0.28f, 0.70f);
    colors[ImGuiCol_BorderShadow]          = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg]               = ImVec4(0.18f, 0.19f, 0.22f, 1.00f);
    colors[ImGuiCol_FrameBgHovered]        = ImVec4(0.24f, 0.25f, 0.29f, 1.00f);
    colors[ImGuiCol_FrameBgActive]         = ImVec4(0.28f, 0.30f, 0.35f, 1.00f);
    colors[ImGuiCol_TitleBg]               = ImVec4(0.10f, 0.10f, 0.12f, 1.00f);
    colors[ImGuiCol_TitleBgActive]         = ImVec4(0.14f, 0.14f, 0.17f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed]      = ImVec4(0.10f, 0.10f, 0.12f, 0.75f);
    colors[ImGuiCol_MenuBarBg]             = ImVec4(0.12f, 0.12f, 0.14f, 1.00f);
    colors[ImGuiCol_ScrollbarBg]           = ImVec4(0.10f, 0.10f, 0.12f, 0.50f);
    colors[ImGuiCol_ScrollbarGrab]         = ImVec4(0.24f, 0.25f, 0.28f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.30f, 0.32f, 0.36f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive]   = ImVec4(0.36f, 0.38f, 0.44f, 1.00f);
    colors[ImGuiCol_CheckMark]             = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_SliderGrab]            = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
    colors[ImGuiCol_SliderGrabActive]      = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_Button]                = ImVec4(0.20f, 0.22f, 0.27f, 1.00f);
    colors[ImGuiCol_ButtonHovered]         = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
    colors[ImGuiCol_ButtonActive]          = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_Header]                = ImVec4(0.20f, 0.22f, 0.27f, 0.70f);
    colors[ImGuiCol_HeaderHovered]         = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
    colors[ImGuiCol_HeaderActive]          = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_Separator]             = ImVec4(0.24f, 0.25f, 0.28f, 0.70f);
    colors[ImGuiCol_SeparatorHovered]      = ImVec4(0.26f, 0.59f, 0.98f, 0.78f);
    colors[ImGuiCol_SeparatorActive]       = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_ResizeGrip]            = ImVec4(0.26f, 0.59f, 0.98f, 0.25f);
    colors[ImGuiCol_ResizeGripHovered]     = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
    colors[ImGuiCol_ResizeGripActive]      = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);
    colors[ImGuiCol_Tab]                   = ImVec4(0.14f, 0.15f, 0.18f, 1.00f);
    colors[ImGuiCol_TabHovered]            = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
    colors[ImGuiCol_TabActive]             = ImVec4(0.20f, 0.22f, 0.27f, 1.00f);
    colors[ImGuiCol_TabUnfocused]          = ImVec4(0.12f, 0.13f, 0.15f, 1.00f);
    colors[ImGuiCol_TabUnfocusedActive]    = ImVec4(0.16f, 0.17f, 0.20f, 1.00f);
    colors[ImGuiCol_DockingPreview]        = ImVec4(0.26f, 0.59f, 0.98f, 0.70f);
    colors[ImGuiCol_DockingEmptyBg]        = ImVec4(0.08f, 0.08f, 0.10f, 1.00f);
    colors[ImGuiCol_PlotLines]             = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
    colors[ImGuiCol_PlotLinesHovered]      = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
    colors[ImGuiCol_PlotHistogram]         = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_PlotHistogramHovered]  = ImVec4(0.36f, 0.69f, 1.00f, 1.00f);
    colors[ImGuiCol_TableHeaderBg]         = ImVec4(0.17f, 0.18f, 0.21f, 1.00f);
    colors[ImGuiCol_TableBorderStrong]     = ImVec4(0.24f, 0.25f, 0.28f, 1.00f);
    colors[ImGuiCol_TableBorderLight]      = ImVec4(0.20f, 0.21f, 0.24f, 1.00f);
    colors[ImGuiCol_TableRowBg]            = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_TableRowBgAlt]         = ImVec4(1.00f, 1.00f, 1.00f, 0.03f);
    colors[ImGuiCol_TextSelectedBg]        = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
    colors[ImGuiCol_DragDropTarget]        = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);
    colors[ImGuiCol_NavHighlight]          = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
}

static void ApplyCyberSlateTheme() {
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    style.WindowRounding    = 3.0f;
    style.ChildRounding     = 2.0f;
    style.FrameRounding     = 3.0f;
    style.PopupRounding     = 3.0f;
    style.ScrollbarRounding = 4.0f;
    style.GrabRounding      = 2.0f;
    style.TabRounding       = 3.0f;

    style.WindowBorderSize  = 1.0f;
    style.FrameBorderSize   = 1.0f;
    style.PopupBorderSize   = 1.0f;

    style.WindowPadding     = ImVec2(9.0f, 9.0f);
    style.FramePadding      = ImVec2(7.0f, 4.0f);
    style.ItemSpacing       = ImVec2(7.0f, 5.0f);
    style.ItemInnerSpacing  = ImVec2(5.0f, 4.0f);
    style.ScrollbarSize     = 13.0f;
    style.GrabMinSize       = 10.0f;

    colors[ImGuiCol_Text]                  = ImVec4(0.88f, 0.92f, 0.96f, 1.00f);
    colors[ImGuiCol_TextDisabled]          = ImVec4(0.40f, 0.45f, 0.52f, 1.00f);
    colors[ImGuiCol_WindowBg]              = ImVec4(0.06f, 0.08f, 0.11f, 1.00f);
    colors[ImGuiCol_ChildBg]               = ImVec4(0.08f, 0.10f, 0.13f, 0.00f);
    colors[ImGuiCol_PopupBg]               = ImVec4(0.08f, 0.10f, 0.14f, 0.98f);
    colors[ImGuiCol_Border]                = ImVec4(0.18f, 0.22f, 0.28f, 0.70f);
    colors[ImGuiCol_BorderShadow]          = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg]               = ImVec4(0.11f, 0.14f, 0.18f, 1.00f);
    colors[ImGuiCol_FrameBgHovered]        = ImVec4(0.15f, 0.19f, 0.25f, 1.00f);
    colors[ImGuiCol_FrameBgActive]         = ImVec4(0.19f, 0.24f, 0.32f, 1.00f);
    colors[ImGuiCol_TitleBg]               = ImVec4(0.05f, 0.06f, 0.08f, 1.00f);
    colors[ImGuiCol_TitleBgActive]         = ImVec4(0.08f, 0.11f, 0.15f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed]      = ImVec4(0.05f, 0.06f, 0.08f, 0.75f);
    colors[ImGuiCol_MenuBarBg]             = ImVec4(0.06f, 0.08f, 0.11f, 1.00f);
    colors[ImGuiCol_ScrollbarBg]           = ImVec4(0.05f, 0.06f, 0.08f, 0.50f);
    colors[ImGuiCol_ScrollbarGrab]         = ImVec4(0.16f, 0.21f, 0.28f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.00f, 0.65f, 0.75f, 0.80f);
    colors[ImGuiCol_ScrollbarGrabActive]   = ImVec4(0.00f, 0.75f, 0.85f, 1.00f);
    colors[ImGuiCol_CheckMark]             = ImVec4(0.00f, 0.82f, 0.85f, 1.00f);
    colors[ImGuiCol_SliderGrab]            = ImVec4(0.00f, 0.75f, 0.80f, 0.80f);
    colors[ImGuiCol_SliderGrabActive]      = ImVec4(0.00f, 0.85f, 0.90f, 1.00f);
    colors[ImGuiCol_Button]                = ImVec4(0.13f, 0.18f, 0.24f, 1.00f);
    colors[ImGuiCol_ButtonHovered]         = ImVec4(0.00f, 0.65f, 0.75f, 0.80f);
    colors[ImGuiCol_ButtonActive]          = ImVec4(0.00f, 0.75f, 0.85f, 1.00f);
    colors[ImGuiCol_Header]                = ImVec4(0.13f, 0.19f, 0.26f, 0.70f);
    colors[ImGuiCol_HeaderHovered]         = ImVec4(0.00f, 0.65f, 0.75f, 0.80f);
    colors[ImGuiCol_HeaderActive]          = ImVec4(0.00f, 0.75f, 0.85f, 1.00f);
    colors[ImGuiCol_Separator]             = ImVec4(0.18f, 0.22f, 0.28f, 0.70f);
    colors[ImGuiCol_SeparatorHovered]      = ImVec4(0.00f, 0.75f, 0.85f, 0.78f);
    colors[ImGuiCol_SeparatorActive]       = ImVec4(0.00f, 0.85f, 0.95f, 1.00f);
    colors[ImGuiCol_ResizeGrip]            = ImVec4(0.00f, 0.75f, 0.85f, 0.25f);
    colors[ImGuiCol_ResizeGripHovered]     = ImVec4(0.00f, 0.75f, 0.85f, 0.67f);
    colors[ImGuiCol_ResizeGripActive]      = ImVec4(0.00f, 0.85f, 0.95f, 0.95f);
    colors[ImGuiCol_Tab]                   = ImVec4(0.08f, 0.11f, 0.15f, 1.00f);
    colors[ImGuiCol_TabHovered]            = ImVec4(0.00f, 0.65f, 0.75f, 0.80f);
    colors[ImGuiCol_TabActive]             = ImVec4(0.13f, 0.18f, 0.25f, 1.00f);
    colors[ImGuiCol_TabUnfocused]          = ImVec4(0.06f, 0.08f, 0.11f, 1.00f);
    colors[ImGuiCol_TabUnfocusedActive]    = ImVec4(0.10f, 0.13f, 0.18f, 1.00f);
    colors[ImGuiCol_DockingPreview]        = ImVec4(0.00f, 0.75f, 0.85f, 0.70f);
    colors[ImGuiCol_DockingEmptyBg]        = ImVec4(0.04f, 0.05f, 0.07f, 1.00f);
    colors[ImGuiCol_PlotLines]             = ImVec4(0.55f, 0.65f, 0.75f, 1.00f);
    colors[ImGuiCol_PlotLinesHovered]      = ImVec4(0.00f, 0.85f, 0.95f, 1.00f);
    colors[ImGuiCol_PlotHistogram]         = ImVec4(0.00f, 0.75f, 0.85f, 1.00f);
    colors[ImGuiCol_PlotHistogramHovered]  = ImVec4(0.00f, 0.85f, 0.95f, 1.00f);
    colors[ImGuiCol_TableHeaderBg]         = ImVec4(0.10f, 0.13f, 0.18f, 1.00f);
    colors[ImGuiCol_TableBorderStrong]     = ImVec4(0.18f, 0.22f, 0.28f, 1.00f);
    colors[ImGuiCol_TableBorderLight]      = ImVec4(0.14f, 0.17f, 0.22f, 1.00f);
    colors[ImGuiCol_TableRowBg]            = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_TableRowBgAlt]         = ImVec4(1.00f, 1.00f, 1.00f, 0.03f);
    colors[ImGuiCol_TextSelectedBg]        = ImVec4(0.00f, 0.75f, 0.85f, 0.35f);
    colors[ImGuiCol_DragDropTarget]        = ImVec4(0.00f, 0.85f, 0.95f, 0.95f);
    colors[ImGuiCol_NavHighlight]          = ImVec4(0.00f, 0.75f, 0.85f, 0.80f);
}

static void ApplyCatppuccinTheme() {
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    style.WindowRounding    = 7.0f;
    style.ChildRounding     = 5.0f;
    style.FrameRounding     = 5.0f;
    style.PopupRounding     = 6.0f;
    style.ScrollbarRounding = 9.0f;
    style.GrabRounding      = 4.0f;
    style.TabRounding       = 5.0f;

    style.WindowBorderSize  = 1.0f;
    style.FrameBorderSize   = 1.0f;
    style.PopupBorderSize   = 1.0f;

    style.WindowPadding     = ImVec2(10.0f, 10.0f);
    style.FramePadding      = ImVec2(8.0f, 5.0f);
    style.ItemSpacing       = ImVec2(8.0f, 6.0f);
    style.ItemInnerSpacing  = ImVec2(6.0f, 4.0f);
    style.ScrollbarSize     = 14.0f;
    style.GrabMinSize       = 12.0f;

    colors[ImGuiCol_Text]                  = ImVec4(0.80f, 0.84f, 0.96f, 1.00f);
    colors[ImGuiCol_TextDisabled]          = ImVec4(0.42f, 0.44f, 0.53f, 1.00f);
    colors[ImGuiCol_WindowBg]              = ImVec4(0.12f, 0.12f, 0.18f, 1.00f);
    colors[ImGuiCol_ChildBg]               = ImVec4(0.14f, 0.14f, 0.21f, 0.00f);
    colors[ImGuiCol_PopupBg]               = ImVec4(0.14f, 0.14f, 0.21f, 0.98f);
    colors[ImGuiCol_Border]                = ImVec4(0.27f, 0.28f, 0.38f, 0.70f);
    colors[ImGuiCol_BorderShadow]          = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg]               = ImVec4(0.19f, 0.20f, 0.28f, 1.00f);
    colors[ImGuiCol_FrameBgHovered]        = ImVec4(0.24f, 0.25f, 0.35f, 1.00f);
    colors[ImGuiCol_FrameBgActive]         = ImVec4(0.29f, 0.30f, 0.42f, 1.00f);
    colors[ImGuiCol_TitleBg]               = ImVec4(0.09f, 0.09f, 0.14f, 1.00f);
    colors[ImGuiCol_TitleBgActive]         = ImVec4(0.12f, 0.12f, 0.18f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed]      = ImVec4(0.09f, 0.09f, 0.14f, 0.75f);
    colors[ImGuiCol_MenuBarBg]             = ImVec4(0.11f, 0.11f, 0.16f, 1.00f);
    colors[ImGuiCol_ScrollbarBg]           = ImVec4(0.09f, 0.09f, 0.14f, 0.50f);
    colors[ImGuiCol_ScrollbarGrab]         = ImVec4(0.27f, 0.28f, 0.38f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.54f, 0.71f, 0.98f, 0.70f);
    colors[ImGuiCol_ScrollbarGrabActive]   = ImVec4(0.71f, 0.75f, 1.00f, 1.00f);
    colors[ImGuiCol_CheckMark]             = ImVec4(0.71f, 0.75f, 1.00f, 1.00f);
    colors[ImGuiCol_SliderGrab]            = ImVec4(0.54f, 0.71f, 0.98f, 0.80f);
    colors[ImGuiCol_SliderGrabActive]      = ImVec4(0.71f, 0.75f, 1.00f, 1.00f);
    colors[ImGuiCol_Button]                = ImVec4(0.23f, 0.24f, 0.34f, 1.00f);
    colors[ImGuiCol_ButtonHovered]         = ImVec4(0.54f, 0.71f, 0.98f, 0.70f);
    colors[ImGuiCol_ButtonActive]          = ImVec4(0.54f, 0.71f, 0.98f, 1.00f);
    colors[ImGuiCol_Header]                = ImVec4(0.24f, 0.25f, 0.35f, 0.70f);
    colors[ImGuiCol_HeaderHovered]         = ImVec4(0.54f, 0.71f, 0.98f, 0.70f);
    colors[ImGuiCol_HeaderActive]          = ImVec4(0.54f, 0.71f, 0.98f, 1.00f);
    colors[ImGuiCol_Separator]             = ImVec4(0.27f, 0.28f, 0.38f, 0.70f);
    colors[ImGuiCol_SeparatorHovered]      = ImVec4(0.54f, 0.71f, 0.98f, 0.78f);
    colors[ImGuiCol_SeparatorActive]       = ImVec4(0.71f, 0.75f, 1.00f, 1.00f);
    colors[ImGuiCol_ResizeGrip]            = ImVec4(0.54f, 0.71f, 0.98f, 0.25f);
    colors[ImGuiCol_ResizeGripHovered]     = ImVec4(0.54f, 0.71f, 0.98f, 0.67f);
    colors[ImGuiCol_ResizeGripActive]      = ImVec4(0.71f, 0.75f, 1.00f, 0.95f);
    colors[ImGuiCol_Tab]                   = ImVec4(0.14f, 0.14f, 0.21f, 1.00f);
    colors[ImGuiCol_TabHovered]            = ImVec4(0.54f, 0.71f, 0.98f, 0.70f);
    colors[ImGuiCol_TabActive]             = ImVec4(0.23f, 0.24f, 0.34f, 1.00f);
    colors[ImGuiCol_TabUnfocused]          = ImVec4(0.11f, 0.11f, 0.16f, 1.00f);
    colors[ImGuiCol_TabUnfocusedActive]    = ImVec4(0.17f, 0.17f, 0.24f, 1.00f);
    colors[ImGuiCol_DockingPreview]        = ImVec4(0.54f, 0.71f, 0.98f, 0.70f);
    colors[ImGuiCol_DockingEmptyBg]        = ImVec4(0.09f, 0.09f, 0.14f, 1.00f);
    colors[ImGuiCol_PlotLines]             = ImVec4(0.65f, 0.68f, 0.82f, 1.00f);
    colors[ImGuiCol_PlotLinesHovered]      = ImVec4(0.71f, 0.75f, 1.00f, 1.00f);
    colors[ImGuiCol_PlotHistogram]         = ImVec4(0.54f, 0.71f, 0.98f, 1.00f);
    colors[ImGuiCol_PlotHistogramHovered]  = ImVec4(0.71f, 0.75f, 1.00f, 1.00f);
    colors[ImGuiCol_TableHeaderBg]         = ImVec4(0.16f, 0.17f, 0.24f, 1.00f);
    colors[ImGuiCol_TableBorderStrong]     = ImVec4(0.27f, 0.28f, 0.38f, 1.00f);
    colors[ImGuiCol_TableBorderLight]      = ImVec4(0.20f, 0.21f, 0.30f, 1.00f);
    colors[ImGuiCol_TableRowBg]            = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_TableRowBgAlt]         = ImVec4(1.00f, 1.00f, 1.00f, 0.03f);
    colors[ImGuiCol_TextSelectedBg]        = ImVec4(0.54f, 0.71f, 0.98f, 0.35f);
    colors[ImGuiCol_DragDropTarget]        = ImVec4(0.71f, 0.75f, 1.00f, 0.95f);
    colors[ImGuiCol_NavHighlight]          = ImVec4(0.54f, 0.71f, 0.98f, 0.80f);
}

static void ApplyClassicDarkTheme() {
    ImGuiStyle& style = ImGui::GetStyle();
    ImGui::StyleColorsDark(&style);

    style.WindowRounding    = 0.0f;
    style.ChildRounding     = 0.0f;
    style.FrameRounding     = 0.0f;
    style.PopupRounding     = 0.0f;
    style.ScrollbarRounding = 9.0f;
    style.GrabRounding      = 0.0f;
    style.TabRounding       = 4.0f;

    style.WindowBorderSize  = 1.0f;
    style.FrameBorderSize   = 0.0f;
    style.PopupBorderSize   = 1.0f;

    style.WindowPadding     = ImVec2(8.0f, 8.0f);
    style.FramePadding      = ImVec2(4.0f, 3.0f);
    style.ItemSpacing       = ImVec2(8.0f, 4.0f);
    style.ItemInnerSpacing  = ImVec2(4.0f, 4.0f);
}

void ImGuiLayer::SetTheme(EditorTheme theme) {
    s_currentTheme = theme;
    if (ImGui::GetCurrentContext() == nullptr) {
        return;
    }
    switch (theme) {
    case EditorTheme::ModernEngine:
        ApplyModernEngineTheme();
        break;
    case EditorTheme::CyberSlate:
        ApplyCyberSlateTheme();
        break;
    case EditorTheme::Catppuccin:
        ApplyCatppuccinTheme();
        break;
    case EditorTheme::ClassicDark:
        ApplyClassicDarkTheme();
        break;
    }

    ImGuiStyle& style = ImGui::GetStyle();
    if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }
}

EditorTheme ImGuiLayer::GetTheme() {
    return s_currentTheme;
}

void ImGuiLayer::Init(HWND hwnd) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;

    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

    // Load clean system font if available
    const char* fontCandidates[] = {
        "C:\\Windows\\Fonts\\segoeui.ttf",
        "C:\\Windows\\Fonts\\meiryo.ttc",
        "C:\\Windows\\Fonts\\msgothic.ttc"
    };
    for (const char* fontPath : fontCandidates) {
        FILE* f = nullptr;
        fopen_s(&f, fontPath, "rb");
        if (f) {
            fclose(f);
            io.Fonts->AddFontFromFileTTF(fontPath, 17.5f, nullptr, io.Fonts->GetGlyphRangesJapanese());
            break;
        }
    }

    // Apply default theme (Modern Engine)
    SetTheme(s_currentTheme);

    ImGui_ImplWin32_Init(hwnd);

    // D3D12 initialization info
    ID3D12Device* device = Renderer::GetDeviceDX12();
    ID3D12DescriptorHeap* srvHeap = Renderer::GetSrvHeapDX12();
    ID3D12CommandQueue* queue = Renderer::GetCommandQueueDX12();

    D3D12_CPU_DESCRIPTOR_HANDLE fontCpuHandle = srvHeap->GetCPUDescriptorHandleForHeapStart();
    D3D12_GPU_DESCRIPTOR_HANDLE fontGpuHandle = srvHeap->GetGPUDescriptorHandleForHeapStart();

    ImGui_ImplDX12_InitInfo initInfo = {};
    initInfo.Device = device;
    initInfo.CommandQueue = queue;
    initInfo.NumFramesInFlight = 2;
    initInfo.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    initInfo.SrvDescriptorHeap = srvHeap;
    initInfo.LegacySingleSrvCpuDescriptor = fontCpuHandle;
    initInfo.LegacySingleSrvGpuDescriptor = fontGpuHandle;

    ImGui_ImplDX12_Init(&initInfo);
}

void ImGuiLayer::Begin() {
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_PassthruCentralNode;
    ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), dockspace_flags);
}

void ImGuiLayer::End() {
    ImGui::Render();
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), Renderer::GetCommandListDX12());
    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
    }
}

void ImGuiLayer::Shutdown() {
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}
