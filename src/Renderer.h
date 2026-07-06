//=====================================================================================
// RendererDX11.h
// Author:Kaito Aoki
// Date:2025/06/23
//=====================================================================================

#ifndef _RENDERER_H
#define _RENDERER_H

#include <stdio.h>
#include <memory>

#include <d3d11.h>
#include <d3d12.h>
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

struct ID3D12Device;
struct ID3D12DescriptorHeap;
struct ID3D12GraphicsCommandList;
struct ID3D12CommandQueue;

#include <d3dcompiler.h>
#pragma comment(lib,"d3dcompiler.lib")

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
    // [新しいマテリアルメンバ] (Deferred / PBR 用)
    XMFLOAT4    BaseColor;
    XMFLOAT4    EmissionColor;
    float       Metallic;
    float       SpecularPbr;
    float       Roughness;
    float       NormalWeight;
    int         ShadingModelID;
    int         FlipU;
    int         FlipV;
    float       DummyPbr;

    // [古いマテリアルメンバ] (既存フォワード用互換)
    XMFLOAT4	Ambient;
    XMFLOAT4	Diffuse;
    XMFLOAT4	Specular;
    XMFLOAT4	Emission;
    float		Shininess;
    BOOL        TextureEnable;
    float		DummyOld[2];
};

struct LIGHT {
    BOOL Enable;
    int DiffuseModel;   // 0:Lambert, 1:half-Lambert, 2:normalized-Lambert
    int ShadingModel;   // 0:Smooth, 1:Toon
    int SpecularModel;  // 0:off, 1:Phong
    
    XMFLOAT4 Direction;
    XMFLOAT4 Diffuse;
    XMFLOAT4 Ambient;
    XMFLOAT4 Position;
    XMFLOAT4 Parameter;
    
    XMFLOAT4 RimColor;
    float RimPower;
    int RimLightModel;
    float DummyLight[2];
};

// D3D12 Resources
struct VERTEX_BUFFER {
    ComPtr<ID3D12Resource> Resource;
    unsigned int Stride;
    unsigned int Size;
};

struct INDEX_BUFFER {
    ComPtr<ID3D12Resource> Resource;
    unsigned int Size;
};

struct TEXTURE {
    ComPtr<ID3D12Resource> Resource;
    unsigned int SRVIndex;
    ~TEXTURE();
};


enum class GraphicsAPI { DirectX11, DirectX12 };

// RendererDX11 namespace has been removed.

namespace RendererDX12 {
    ID3D12Device* GetDevice();
    ID3D12DescriptorHeap* GetSrvHeap();
    int GetFrameCount();
    ID3D12GraphicsCommandList* GetCommandList();
    ID3D12CommandQueue* GetCommandQueue();
    ID3D12RootSignature* GetRootSignature();
    
    ID3D11Device* GetDevice11();
    ID3D11DeviceContext* GetContext11();

    bool Init(HWND hwnd);
    void Shutdown();
    void BeginFrame();
    void EndFrame();
    
    bool InitSceneRenderTarget(int width, int height);
    void BeginSceneRender();
    void* GetSceneSRV();
    void ResizeSceneBuffer(float width, float height);

    bool InitGameRenderTarget(int width, int height);
    void BeginGameRender();
    void* GetGameSRV();
    void ResizeGameBuffer(float width, float height);

    std::unique_ptr<VERTEX_BUFFER> CreateVertexBuffer(unsigned int stride, unsigned int size);
    std::unique_ptr<INDEX_BUFFER> CreateIndexBuffer(unsigned int size);
    unsigned int CreateShaderResourceView(ID3D12Resource* resource);
    void ReleaseShaderResourceView(unsigned int index);
    void SetConstant(int slot, const void* data, unsigned int size);
    void SetTexture(int slot, const TEXTURE* texture);
    void DrawFullScreenQuad();
    bool IsGeometryPass();
    void SetGeometryPass(bool active);
    void ApplyDeferredLighting();
    void* GetGBufferSRV(int bufferIndex, bool isGame);
    void PrintDebugMessages();
    D3D12_CULL_MODE GetCullMode();
    bool GetDepthEnable();
    bool GetDepthWrite();
    void SetDepthEnable(bool enable);
    void SetDepthReadOnly();
    void SetCullModeBack();
    void SetCullModeFront();
    void SetCullModeNone();
    void SetViewMatrix(XMMATRIX view);
    void SetProjectionMatrix(XMMATRIX projection);
    void SetLight(LIGHT light);
    void SetCameraPosition(XMFLOAT4 cameraPos);
    void BindShaderConstants();
    void WaitForLastSubmittedFrame();
}

namespace Renderer {

    void SetGraphicsAPI(GraphicsAPI api);
    GraphicsAPI GetGraphicsAPI();
    
    // Abstracted DX11 context methods
    void IASetVertexBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppVertexBuffers, const UINT* pStrides, const UINT* pOffsets);
    void IASetIndexBuffer(ID3D11Buffer* pIndexBuffer, DXGI_FORMAT Format, UINT Offset);
    void IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY Topology);
    void IASetInputLayout(ID3D11InputLayout* pInputLayout);
    void VSSetShader(ID3D11VertexShader* pVertexShader, ID3D11ClassInstance* const* ppClassInstances, UINT NumClassInstances);
    void PSSetShader(ID3D11PixelShader* pPixelShader, ID3D11ClassInstance* const* ppClassInstances, UINT NumClassInstances);
    void PSSetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView* const* ppShaderResourceViews);
    void Draw(UINT VertexCount, UINT StartVertexLocation);
    void DrawIndexed(UINT IndexCount, UINT StartIndexLocation, INT BaseVertexLocation);
    HRESULT Map(ID3D11Resource* pResource, UINT Subresource, D3D11_MAP MapType, UINT MapFlags, D3D11_MAPPED_SUBRESOURCE* pMappedResource);
    void Unmap(ID3D11Resource* pResource, UINT Subresource);
    
    HRESULT CreateVertexShader(const void* pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage* pClassLinkage, ID3D11VertexShader** ppVertexShader);
    HRESULT CreatePixelShader(const void* pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage* pClassLinkage, ID3D11PixelShader** ppPixelShader);
    HRESULT CreateInputLayout(const D3D11_INPUT_ELEMENT_DESC* pInputElementDescs, UINT NumElements, const void* pShaderBytecodeWithInputSignature, SIZE_T BytecodeLength, ID3D11InputLayout** ppInputLayout);
    HRESULT CreateBuffer(const D3D11_BUFFER_DESC* pDesc, const D3D11_SUBRESOURCE_DATA* pInitialData, ID3D11Buffer** ppBuffer);

    bool Init(HWND hwnd);
    void Shutdown();
    void BeginFrame();
    void EndFrame();
    ID3D11Device* GetDevice();
    ID3D11DeviceContext* GetContext();

    void SetDepthEnable(bool enable);
    void SetDepthReadOnly();

    void SetWorldMatrix(XMMATRIX world);
    void SetViewMatrix(XMMATRIX view);
    void SetProjectionMatrix(XMMATRIX projection);

	void SetMaterial(MATERIAL material);
	void SetLight(LIGHT light);
    void SetCameraPosition(XMFLOAT4 cameraPos);

    void SetWorldProjection2D();
	void SetWorldProjection3D();

    void CreateVertexShader();
    void CreatePixelShader();
	void CreateVertexShader(ID3D11VertexShader** vertexShader, ID3D11InputLayout** vertexLayout, const char* fileName);
	void CreatePixelShader(ID3D11PixelShader** pixelShader, const char* fileName);

    void SetCullModeBack();
    void SetCullModeFront();
    void SetCullModeNone();
    
    void SetDefaultShader();
	void ShaderReload();

    bool InitSceneRenderTarget(int width, int height);  //シーン用バッファ
    void BeginSceneRender(); //レンダリング開始
    void* GetSceneSRV(); //ImGuiに渡すテクスチャ取得

    float GetSceneWidth();
    float GetSceneHeight();

    void ResizeSceneBuffer(float width, float height);

    bool InitGameRenderTarget(int width, int height);  //ゲーム用バッファ
    void BeginGameRender();
    void* GetGameSRV();

    float GetGameWidth();
    float GetGameHeight();

    void ResizeGameBuffer(float width, float height);

    // D3D12 Resource Helpers
    std::unique_ptr<VERTEX_BUFFER> CreateVertexBuffer(unsigned int stride, unsigned int size);
    std::unique_ptr<INDEX_BUFFER> CreateIndexBuffer(unsigned int size);
    void ReleaseTextureSrv(unsigned int index);
    void SetConstant(int slot, const void* data, unsigned int size);
    void SetTexture(int slot, const TEXTURE* texture);
    void DrawFullScreenQuad();
    bool IsGeometryPass();
    void SetGeometryPass(bool active);
    void ApplyDeferredLighting();
    void* GetGBufferSRV(int bufferIndex, bool isGame);
}

struct ID3D12Device;
struct ID3D12DescriptorHeap;
struct ID3D12GraphicsCommandList;
struct ID3D12CommandQueue;

namespace Renderer {
    ID3D12Device* GetDeviceDX12();
    ID3D12DescriptorHeap* GetSrvHeapDX12();
    int GetFrameCountDX12();
    ID3D12GraphicsCommandList* GetCommandListDX12();
    ID3D12CommandQueue* GetCommandQueueDX12();
    ID3D12RootSignature* GetRootSignatureDX12();
    void PrintDebugMessagesDX12();
    D3D12_CULL_MODE GetCullModeDX12();
    bool GetDepthEnableDX12();
    bool GetDepthWriteDX12();
    void BindShaderConstantsDX12();
    void FlushGPUDX12();
}

#endif // !_RENDERER_H
