import codecs

# 1. Update Renderer.h
with codecs.open(r'c:\Users\kaito\source\repos\KTEngine\src\Renderer.h', 'r', 'cp932') as f:
    renderer_h = f.read()

renderer_h = renderer_h.replace('namespace Renderer {', '''
enum class GraphicsAPI { DirectX11, DirectX12 };

namespace RendererDX11 {
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
    HRESULT CreateBuffer(const D3D11_BUFFER_DESC* pDesc, const D3D11_SUBRESOURCE_DATA* pInitialData, ID3D11Buffer** ppBuffer);
}

namespace RendererDX12 {
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
''')

with codecs.open(r'c:\Users\kaito\source\repos\KTEngine\src\Renderer.h', 'w', 'cp932') as f:
    f.write(renderer_h)

# 2. Rename RendererDX11.cpp's includes and namespace
with codecs.open(r'c:\Users\kaito\source\repos\KTEngine\src\RendererDX11.cpp', 'r', 'cp932') as f:
    cpp = f.read()

cpp = cpp.replace('#include "RendererDX11.h"', '#include "Renderer.h"')
cpp = cpp.replace('namespace Renderer {', 'namespace RendererDX11 {')

cpp += '''
namespace RendererDX11 {
    void IASetVertexBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppVertexBuffers, const UINT* pStrides, const UINT* pOffsets) {
        _context->IASetVertexBuffers(StartSlot, NumBuffers, ppVertexBuffers, pStrides, pOffsets);
    }
    void IASetIndexBuffer(ID3D11Buffer* pIndexBuffer, DXGI_FORMAT Format, UINT Offset) {
        _context->IASetIndexBuffer(pIndexBuffer, Format, Offset);
    }
    void IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY Topology) {
        _context->IASetPrimitiveTopology(Topology);
    }
    void IASetInputLayout(ID3D11InputLayout* pInputLayout) {
        _context->IASetInputLayout(pInputLayout);
    }
    void VSSetShader(ID3D11VertexShader* pVertexShader, ID3D11ClassInstance* const* ppClassInstances, UINT NumClassInstances) {
        _context->VSSetShader(pVertexShader, ppClassInstances, NumClassInstances);
    }
    void PSSetShader(ID3D11PixelShader* pPixelShader, ID3D11ClassInstance* const* ppClassInstances, UINT NumClassInstances) {
        _context->PSSetShader(pPixelShader, ppClassInstances, NumClassInstances);
    }
    void PSSetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView* const* ppShaderResourceViews) {
        _context->PSSetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
    }
    void Draw(UINT VertexCount, UINT StartVertexLocation) {
        _context->Draw(VertexCount, StartVertexLocation);
    }
    void DrawIndexed(UINT IndexCount, UINT StartIndexLocation, INT BaseVertexLocation) {
        _context->DrawIndexed(IndexCount, StartIndexLocation, BaseVertexLocation);
    }
    HRESULT Map(ID3D11Resource* pResource, UINT Subresource, D3D11_MAP MapType, UINT MapFlags, D3D11_MAPPED_SUBRESOURCE* pMappedResource) {
        return _context->Map(pResource, Subresource, MapType, MapFlags, pMappedResource);
    }
    void Unmap(ID3D11Resource* pResource, UINT Subresource) {
        _context->Unmap(pResource, Subresource);
    }
    HRESULT CreateBuffer(const D3D11_BUFFER_DESC* pDesc, const D3D11_SUBRESOURCE_DATA* pInitialData, ID3D11Buffer** ppBuffer) {
        return _device->CreateBuffer(pDesc, pInitialData, ppBuffer);
    }
}
'''

with codecs.open(r'c:\Users\kaito\source\repos\KTEngine\src\RendererDX11.cpp', 'w', 'cp932') as f:
    f.write(cpp)
