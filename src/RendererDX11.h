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

#include "DirectXTex.h"

#if _DEBUG
#pragma comment(lib,"DirectXTex_Debug.lib")
#else
#pragma comment(lib,"DirectXTex_Release.lib")
#endif

#define SCREEN_WIDTH (1920)
#define SCREEN_HEIGHT (1080)

// 頂点構造体
struct Vertex {
    XMFLOAT3 position;
    XMFLOAT3 normal;
    XMFLOAT4 color;
    XMFLOAT2 uv;
};

// マテリアル構造体
struct MATERIAL
{
    XMFLOAT4	Ambient;
    XMFLOAT4	Diffuse;
    XMFLOAT4	Specular;
    XMFLOAT4	Emission;
    float		Shininess;
    BOOL        TextureEnable;
    float		Dummy[2];
};

struct LIGHT {
	BOOL	Enable;
    BOOL	Dummy[3];
	XMFLOAT4 Direction;
	XMFLOAT4 Diffuse;
	XMFLOAT4 Ambient;
};

namespace RendererDX11 {
    bool Init(HWND hwnd);
    void Shutdown();
    void BeginFrame();
    void EndFrame();
    ID3D11Device* GetDevice();
    ID3D11DeviceContext* GetContext();

    void SetDepthEnable(bool enable);

    void SetWorldMatrix(XMMATRIX world);
    void SetViewMatrix(XMMATRIX view);
    void SetProjectionMatrix(XMMATRIX projection);

	void SetMaterial(MATERIAL material);
	void SetLight(LIGHT light);

    void SetWorldProjection2D();
	void SetWorldProjection3D();

    void CreateVertexShader();
    void CreatePixelShader();

    void SetCullModeBack();
    void SetCullModeFront();
    void SetCullModeNone();
}

#endif // !_RENDERERDX11_H
