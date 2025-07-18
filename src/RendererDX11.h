//=====================================================================================
// RendererDX11.h
// Author:Kaito Aoki
// Date:2025/06/23
//=====================================================================================

#ifndef _RENDERERDX11_H
#define _RENDERERDX11_H

#include <stdio.h>

#include <d3d11.h>
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;

#include <DirectXMath.h>
using namespace DirectX;

// í∏ì_ç\ë¢ëÃ
struct Vertex {
    XMFLOAT3 position;
    XMFLOAT3 normal;
    XMFLOAT4 color;
    XMFLOAT2 uv;
};

namespace RendererDX11 {
    bool Init(HWND hwnd);
    void Shutdown();
    void BeginFrame();
    void EndFrame();
    ID3D11Device* GetDevice();
    ID3D11DeviceContext* GetContext();

    void SetWorldMatrix(XMMATRIX world);
    void SetViewMatrix(XMMATRIX view);
    void SetProjectionMatrix(XMMATRIX projection);

    void SetWorldProjection2D();
	void SetWorldProjection3D();

    void CreateVertexShader();
    void CreatePixelShader();
}

#endif // !_RENDERERDX11_H
