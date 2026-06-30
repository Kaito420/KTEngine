#include "Renderer.h"
#include <iostream>

GraphicsAPI g_CurrentAPI = GraphicsAPI::DirectX12;

namespace Renderer {
    void SetGraphicsAPI(GraphicsAPI api) {
        g_CurrentAPI = api;
    }

    GraphicsAPI GetGraphicsAPI() {
        return g_CurrentAPI;
    }

    bool Init(HWND hwnd) {
        return RendererDX12::Init(hwnd);
    }

    void Shutdown() {
        RendererDX12::Shutdown();
    }

    void BeginFrame() {
        RendererDX12::BeginFrame();
    }

    void EndFrame() {
        RendererDX12::EndFrame();
    }

    ID3D11Device* GetDevice() {
        return nullptr;
    }

    ID3D11DeviceContext* GetContext() {
        return nullptr;
    }

    void SetDepthEnable(bool enable) {}
    void SetDepthReadOnly() {}
    void SetWorldMatrix(XMMATRIX world) {}
    void SetViewMatrix(XMMATRIX view) {}
    void SetProjectionMatrix(XMMATRIX projection) {}
    void SetMaterial(MATERIAL material) {}
    void SetLight(LIGHT light) {}
    void SetCameraPosition(XMFLOAT4 cameraPos) {}
    void SetWorldProjection2D() {}
    void SetWorldProjection3D() {}
    void CreateVertexShader() {}
    void CreatePixelShader() {}
    void CreateVertexShader(ID3D11VertexShader** vertexShader, ID3D11InputLayout** vertexLayout, const char* fileName) {}
    void CreatePixelShader(ID3D11PixelShader** pixelShader, const char* fileName) {}
    void SetCullModeBack() {}
    void SetCullModeFront() {}
    void SetCullModeNone() {}
    void SetDefaultShader() {}
    void ShaderReload() {}

    bool InitSceneRenderTarget(int width, int height) {
        return RendererDX12::InitSceneRenderTarget(width, height);
    }

    void BeginSceneRender() {
        RendererDX12::BeginSceneRender();
    }

    void* GetSceneSRV() {
        return RendererDX12::GetSceneSRV();
    }

    float GetSceneWidth() { return 1280.0f; }
    float GetSceneHeight() { return 720.0f; }
    void ResizeSceneBuffer(float width, float height) {
        RendererDX12::ResizeSceneBuffer(width, height);
    }

    bool InitGameRenderTarget(int width, int height) {
        return RendererDX12::InitGameRenderTarget(width, height);
    }

    void BeginGameRender() {
        RendererDX12::BeginGameRender();
    }

    void* GetGameSRV() {
        return RendererDX12::GetGameSRV();
    }

    float GetGameWidth() { return 1280.0f; }
    float GetGameHeight() { return 720.0f; }
    void ResizeGameBuffer(float width, float height) {
        RendererDX12::ResizeGameBuffer(width, height);
    }

    // Abstracted DX11 context methods (Dummies)
    void IASetVertexBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppVertexBuffers, const UINT* pStrides, const UINT* pOffsets) {}
    void IASetIndexBuffer(ID3D11Buffer* pIndexBuffer, DXGI_FORMAT Format, UINT Offset) {}
    void IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY Topology) {}
    void IASetInputLayout(ID3D11InputLayout* pInputLayout) {}
    void VSSetShader(ID3D11VertexShader* pVertexShader, ID3D11ClassInstance* const* ppClassInstances, UINT NumClassInstances) {}
    void PSSetShader(ID3D11PixelShader* pPixelShader, ID3D11ClassInstance* const* ppClassInstances, UINT NumClassInstances) {}
    void PSSetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView* const* ppShaderResourceViews) {}
    void Draw(UINT VertexCount, UINT StartVertexLocation) {}
    void DrawIndexed(UINT IndexCount, UINT StartIndexLocation, INT BaseVertexLocation) {}
    HRESULT Map(ID3D11Resource* pResource, UINT Subresource, D3D11_MAP MapType, UINT MapFlags, D3D11_MAPPED_SUBRESOURCE* pMappedResource) { return S_OK; }
    void Unmap(ID3D11Resource* pResource, UINT Subresource) {}

    HRESULT CreateVertexShader(const void* pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage* pClassLinkage, ID3D11VertexShader** ppVertexShader) { return S_OK; }
    HRESULT CreatePixelShader(const void* pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage* pClassLinkage, ID3D11PixelShader** ppPixelShader) { return S_OK; }
    HRESULT CreateInputLayout(const D3D11_INPUT_ELEMENT_DESC* pInputElementDescs, UINT NumElements, const void* pShaderBytecodeWithInputSignature, SIZE_T BytecodeLength, ID3D11InputLayout** ppInputLayout) { return S_OK; }
    HRESULT CreateBuffer(const D3D11_BUFFER_DESC* pDesc, const D3D11_SUBRESOURCE_DATA* pInitialData, ID3D11Buffer** ppBuffer) { return S_OK; }
}

namespace Renderer {
    ID3D12Device* GetDeviceDX12() { return RendererDX12::GetDevice(); }
    ID3D12DescriptorHeap* GetSrvHeapDX12() { return RendererDX12::GetSrvHeap(); }
    int GetFrameCountDX12() { return RendererDX12::GetFrameCount(); }
    ID3D12GraphicsCommandList* GetCommandListDX12() { return RendererDX12::GetCommandList(); }
    ID3D12CommandQueue* GetCommandQueueDX12() { return RendererDX12::GetCommandQueue(); }
}
