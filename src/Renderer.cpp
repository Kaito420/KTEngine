#include "Renderer.h"
#include <iostream>

GraphicsAPI g_CurrentAPI = GraphicsAPI::DirectX11;

namespace Renderer {
    void SetGraphicsAPI(GraphicsAPI api) {
        g_CurrentAPI = api;
    }

    GraphicsAPI GetGraphicsAPI() {
        return g_CurrentAPI;
    }

    bool Init(HWND hwnd) {
        if (g_CurrentAPI == GraphicsAPI::DirectX11) {
            return RendererDX11::Init(hwnd);
        } else {
            return RendererDX12::Init(hwnd);
        }
    }

    void Shutdown() {
        if (g_CurrentAPI == GraphicsAPI::DirectX11) {
            RendererDX11::Shutdown();
        } else {
            RendererDX12::Shutdown();
        }
    }

    void BeginFrame() {
        if (g_CurrentAPI == GraphicsAPI::DirectX11) {
            RendererDX11::BeginFrame();
        } else {
            RendererDX12::BeginFrame();
        }
    }

    void EndFrame() {
        if (g_CurrentAPI == GraphicsAPI::DirectX11) {
            RendererDX11::EndFrame();
        } else {
            RendererDX12::EndFrame();
        }
    }

    ID3D11Device* GetDevice() {
        return RendererDX11::GetDevice();
    }

    ID3D11DeviceContext* GetContext() {
        return RendererDX11::GetContext();
    }

    void SetDepthEnable(bool enable) {
        RendererDX11::SetDepthEnable(enable);
    }

    void SetDepthReadOnly() {
        RendererDX11::SetDepthReadOnly();
    }

    void SetWorldMatrix(XMMATRIX world) {
        RendererDX11::SetWorldMatrix(world);
    }

    void SetViewMatrix(XMMATRIX view) {
        RendererDX11::SetViewMatrix(view);
    }

    void SetProjectionMatrix(XMMATRIX projection) {
        RendererDX11::SetProjectionMatrix(projection);
    }

    void SetMaterial(MATERIAL material) {
        RendererDX11::SetMaterial(material);
    }

    void SetLight(LIGHT light) {
        RendererDX11::SetLight(light);
    }

    void SetCameraPosition(XMFLOAT4 cameraPos) {
        RendererDX11::SetCameraPosition(cameraPos);
    }

    void SetWorldProjection2D() {
        RendererDX11::SetWorldProjection2D();
    }

    void SetWorldProjection3D() {
        RendererDX11::SetWorldProjection3D();
    }

    void CreateVertexShader() {
        RendererDX11::CreateVertexShader();
    }

    void CreatePixelShader() {
        RendererDX11::CreatePixelShader();
    }

    void CreateVertexShader(ID3D11VertexShader** vertexShader, ID3D11InputLayout** vertexLayout, const char* fileName) {
        RendererDX11::CreateVertexShader(vertexShader, vertexLayout, fileName);
    }

    void CreatePixelShader(ID3D11PixelShader** pixelShader, const char* fileName) {
        RendererDX11::CreatePixelShader(pixelShader, fileName);
    }

    void SetCullModeBack() {
        RendererDX11::SetCullModeBack();
    }

    void SetCullModeFront() {
        RendererDX11::SetCullModeFront();
    }

    void SetCullModeNone() {
        RendererDX11::SetCullModeNone();
    }

    void SetDefaultShader() {
        RendererDX11::SetDefaultShader();
    }

    void ShaderReload() {
        RendererDX11::ShaderReload();
    }

    bool InitSceneRenderTarget(int width, int height) {
        return RendererDX11::InitSceneRenderTarget(width, height);
    }

    void BeginSceneRender() {
        RendererDX11::BeginSceneRender();
    }

    void* GetSceneSRV() {
        return RendererDX11::GetSceneSRV();
    }

    float GetSceneWidth() {
        return RendererDX11::GetSceneWidth();
    }

    float GetSceneHeight() {
        return RendererDX11::GetSceneHeight();
    }

    void ResizeSceneBuffer(float width, float height) {
        RendererDX11::ResizeSceneBuffer(width, height);
    }

    bool InitGameRenderTarget(int width, int height) {
        if (g_CurrentAPI == GraphicsAPI::DirectX12) {
            return true; // Use DX11 logic inside DX12 if it's wrapped, but actually we use DX11 on 12
        }
        return RendererDX11::InitGameRenderTarget(width, height);
    }

    void BeginGameRender() {
        if (g_CurrentAPI == GraphicsAPI::DirectX12) {
            // No direct DX12 logic for Game render yet, it uses DX11 wrapped resources
        }
        RendererDX11::BeginGameRender();
    }

    void* GetGameSRV() {
        return RendererDX11::GetGameSRV();
    }

    float GetGameWidth() {
        return RendererDX11::GetGameWidth();
    }

    float GetGameHeight() {
        return RendererDX11::GetGameHeight();
    }

    void ResizeGameBuffer(float width, float height) {
        RendererDX11::ResizeGameBuffer(width, height);
    }

    // Abstracted DX11 context methods
    void IASetVertexBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppVertexBuffers, const UINT* pStrides, const UINT* pOffsets) {
        RendererDX11::IASetVertexBuffers(StartSlot, NumBuffers, ppVertexBuffers, pStrides, pOffsets);
    }

    void IASetIndexBuffer(ID3D11Buffer* pIndexBuffer, DXGI_FORMAT Format, UINT Offset) {
        RendererDX11::IASetIndexBuffer(pIndexBuffer, Format, Offset);
    }

    void IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY Topology) {
        RendererDX11::IASetPrimitiveTopology(Topology);
    }

    void IASetInputLayout(ID3D11InputLayout* pInputLayout) {
        RendererDX11::IASetInputLayout(pInputLayout);
    }

    void VSSetShader(ID3D11VertexShader* pVertexShader, ID3D11ClassInstance* const* ppClassInstances, UINT NumClassInstances) {
        RendererDX11::VSSetShader(pVertexShader, ppClassInstances, NumClassInstances);
    }

    void PSSetShader(ID3D11PixelShader* pPixelShader, ID3D11ClassInstance* const* ppClassInstances, UINT NumClassInstances) {
        RendererDX11::PSSetShader(pPixelShader, ppClassInstances, NumClassInstances);
    }

    void PSSetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView* const* ppShaderResourceViews) {
        RendererDX11::PSSetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
    }

    void Draw(UINT VertexCount, UINT StartVertexLocation) {
        RendererDX11::Draw(VertexCount, StartVertexLocation);
    }

    void DrawIndexed(UINT IndexCount, UINT StartIndexLocation, INT BaseVertexLocation) {
        RendererDX11::DrawIndexed(IndexCount, StartIndexLocation, BaseVertexLocation);
    }

    HRESULT Map(ID3D11Resource* pResource, UINT Subresource, D3D11_MAP MapType, UINT MapFlags, D3D11_MAPPED_SUBRESOURCE* pMappedResource) {
        return RendererDX11::Map(pResource, Subresource, MapType, MapFlags, pMappedResource);
    }

    void Unmap(ID3D11Resource* pResource, UINT Subresource) {
        RendererDX11::Unmap(pResource, Subresource);
    }

    
    HRESULT CreateVertexShader(const void* pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage* pClassLinkage, ID3D11VertexShader** ppVertexShader) {
        return RendererDX11::CreateVertexShader(pShaderBytecode, BytecodeLength, pClassLinkage, ppVertexShader);
    }
    HRESULT CreatePixelShader(const void* pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage* pClassLinkage, ID3D11PixelShader** ppPixelShader) {
        return RendererDX11::CreatePixelShader(pShaderBytecode, BytecodeLength, pClassLinkage, ppPixelShader);
    }
    HRESULT CreateInputLayout(const D3D11_INPUT_ELEMENT_DESC* pInputElementDescs, UINT NumElements, const void* pShaderBytecodeWithInputSignature, SIZE_T BytecodeLength, ID3D11InputLayout** ppInputLayout) {
        return RendererDX11::CreateInputLayout(pInputElementDescs, NumElements, pShaderBytecodeWithInputSignature, BytecodeLength, ppInputLayout);
    }
    HRESULT CreateBuffer(const D3D11_BUFFER_DESC* pDesc, const D3D11_SUBRESOURCE_DATA* pInitialData, ID3D11Buffer** ppBuffer) {
        return RendererDX11::CreateBuffer(pDesc, pInitialData, ppBuffer);
    }
}

namespace Renderer {
    ID3D12Device* GetDeviceDX12() { return RendererDX12::GetDevice(); }
    ID3D12DescriptorHeap* GetSrvHeapDX12() { return RendererDX12::GetSrvHeap(); }
    int GetFrameCountDX12() { return RendererDX12::GetFrameCount(); }
    ID3D12GraphicsCommandList* GetCommandListDX12() { return RendererDX12::GetCommandList(); }
    ID3D12CommandQueue* GetCommandQueueDX12() { return RendererDX12::GetCommandQueue(); }
}
