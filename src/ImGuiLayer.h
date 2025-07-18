//=====================================================================================
// ImGuiLayer.h
// Author:Kaito Aoki
// Date:2025/06/23
//=====================================================================================

#ifndef _IMGUILAYER_H
#define _IMGUILAYER_H

#include <windows.h>
#include <d3d11.h>

namespace ImGuiLayer {
    void Init(HWND hwnd, ID3D11Device* device, ID3D11DeviceContext* context);
    void Begin();
    void End();
    void Shutdown();
}
#endif // !_IMGUILAYER_H
