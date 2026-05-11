//=====================================================================================
// RendererDX11.h
// Author:Kaito Aoki
// Date:2025/06/23
//=====================================================================================

#ifndef _RENDERER_H
#define _RENDERER_H

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
    XMFLOAT4 Position;
    XMFLOAT4 Parameter;
};


enum class GraphicsAPI { DirectX11, DirectX12 };

namespace RendererDX11 {
    ID3D11DepthStencilView* GetDSV();
    void SetExternalDevice(ID3D11Device* dev, ID3D11DeviceContext* ctx);
    bool InitState(UINT width, UINT height);
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

    bool InitSceneRenderTarget(int width, int height);
    void BeginSceneRender();
    void* GetSceneSRV();

    float GetSceneWidth();
    float GetSceneHeight();

    void ResizeSceneBuffer(float width, float height);
    
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
    
    HRESULT CreateVertexShader(const void* pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage* pClassLinkage, ID3D11VertexShader** ppVertexShader);
    HRESULT CreatePixelShader(const void* pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage* pClassLinkage, ID3D11PixelShader** ppPixelShader);
    HRESULT CreateInputLayout(const D3D11_INPUT_ELEMENT_DESC* pInputElementDescs, UINT NumElements, const void* pShaderBytecodeWithInputSignature, SIZE_T BytecodeLength, ID3D11InputLayout** ppInputLayout);
    HRESULT CreateBuffer(const D3D11_BUFFER_DESC* pDesc, const D3D11_SUBRESOURCE_DATA* pInitialData, ID3D11Buffer** ppBuffer);
}

namespace RendererDX12 {
    ID3D12Device* GetDevice();
    ID3D12DescriptorHeap* GetSrvHeap();
    int GetFrameCount();
    ID3D12GraphicsCommandList* GetCommandList();
    ID3D12CommandQueue* GetCommandQueue();
    
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
}

#endif // !_RENDERER_H
