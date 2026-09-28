//=====================================================================================
// ImGuiLayer.h
// Author:Kaito Aoki
// Date:2025/06/23
//=====================================================================================

#ifndef _IMGUILAYER_H
#define _IMGUILAYER_H

#include <windows.h>
#include <d3d11.h>

enum class EditorTheme {
    ModernEngine = 0,  // Plan 1: UE5 / Rider style dark
    CyberSlate   = 1,  // Plan 2: GitHub Dark / VSCode style
    Catppuccin   = 2,  // Plan 3: Nord / Catppuccin Mocha style
    ClassicDark  = 3   // Classic ImGui Dark
};

namespace ImGuiLayer {
    void Init(HWND hwnd);
    void Begin();
    void End();
    void Shutdown();

    void SetTheme(EditorTheme theme);
    EditorTheme GetTheme();
}
#endif // !_IMGUILAYER_H
