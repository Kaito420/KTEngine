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

    void SetDepthEnable(bool enable) { RendererDX12::SetDepthEnable(enable); }
    void SetDepthReadOnly() { RendererDX12::SetDepthReadOnly(); }
    void SetWorldMatrix(XMMATRIX world) {
        XMMATRIX transposed = XMMatrixTranspose(world);
        SetConstant(0, &transposed, sizeof(transposed));
    }
    void SetViewMatrix(XMMATRIX view) {
        RendererDX12::SetViewMatrix(view);
    }
    void SetProjectionMatrix(XMMATRIX projection) {
        RendererDX12::SetProjectionMatrix(projection);
    }
    void SetMaterial(MATERIAL material) {
        SetConstant(3, &material, sizeof(material));
    }
    void SetLight(LIGHT light) {
        RendererDX12::SetLight(light);
    }
    void SetCameraPosition(XMFLOAT4 cameraPos) {
        RendererDX12::SetCameraPosition(cameraPos);
    }
    void SetWorldProjection2D() {}
    void SetWorldProjection3D() {}
    void CreateVertexShader() {}
    void CreatePixelShader() {}
    void CreateVertexShader(ID3D11VertexShader** vertexShader, ID3D11InputLayout** vertexLayout, const char* fileName) {}
    void CreatePixelShader(ID3D11PixelShader** pixelShader, const char* fileName) {}
    void SetCullModeBack() { RendererDX12::SetCullModeBack(); }
    void SetCullModeFront() { RendererDX12::SetCullModeFront(); }
    void SetCullModeNone() { RendererDX12::SetCullModeNone(); }
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
    ID3D12RootSignature* GetRootSignatureDX12() { return RendererDX12::GetRootSignature(); }
    void PrintDebugMessagesDX12() { RendererDX12::PrintDebugMessages(); }
    D3D12_CULL_MODE GetCullModeDX12() { return RendererDX12::GetCullMode(); }
    bool GetDepthEnableDX12() { return RendererDX12::GetDepthEnable(); }
    bool GetDepthWriteDX12() { return RendererDX12::GetDepthWrite(); }
    void BindShaderConstantsDX12() { RendererDX12::BindShaderConstants(); }
    void FlushGPUDX12() { RendererDX12::WaitForLastSubmittedFrame(); }

    std::unique_ptr<VERTEX_BUFFER> CreateVertexBuffer(unsigned int stride, unsigned int size) {
        return RendererDX12::CreateVertexBuffer(stride, size);
    }

    std::unique_ptr<INDEX_BUFFER> CreateIndexBuffer(unsigned int size) {
        return RendererDX12::CreateIndexBuffer(size);
    }

    void ReleaseTextureSrv(unsigned int index) {
        RendererDX12::ReleaseShaderResourceView(index);
    }

    void SetConstant(int slot, const void* data, unsigned int size) {
        RendererDX12::SetConstant(slot, data, size);
    }

    void SetTexture(int slot, const TEXTURE* texture) {
        RendererDX12::SetTexture(slot, texture);
    }

    void DrawFullScreenQuad() {
        RendererDX12::DrawFullScreenQuad();
    }

    bool IsGeometryPass() {
        return RendererDX12::IsGeometryPass();
    }

    void SetGeometryPass(bool active) {
        RendererDX12::SetGeometryPass(active);
    }

    void ApplyDeferredLighting() {
        RendererDX12::ApplyDeferredLighting();
    }
}

TEXTURE::~TEXTURE() {
    Renderer::ReleaseTextureSrv(SRVIndex);
}
