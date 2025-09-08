//=====================================================================================
// RendererDX11.cpp
// Author:Kaito Aoki
// Date:2025/06/23
//=====================================================================================

#include "RendererDX11.h"
#include <dxgi.h>
#include <io.h>

namespace {
    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
    IDXGISwapChain* swapChain = nullptr;
    ID3D11RenderTargetView* rtv = nullptr;

    ID3D11Buffer* worldBuffer = nullptr;
    ID3D11Buffer* viewBuffer = nullptr;
    ID3D11Buffer* projectionBuffer = nullptr;

    ID3D11VertexShader* vertexShader = nullptr;
    ID3D11InputLayout* vertexLayout = nullptr;
    ID3D11PixelShader* pixelShader = nullptr;

}

bool RendererDX11::Init(HWND hwnd) {

    //スワップチェーン生成
    DXGI_SWAP_CHAIN_DESC scd = {};
    scd.BufferCount = 1;
    scd.BufferDesc.Width = 0;
    scd.BufferDesc.Height = 0;
    scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow = hwnd;
    scd.SampleDesc.Count = 1;
    scd.Windowed = TRUE;

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0,
        D3D11_SDK_VERSION, &scd, &swapChain, &device, nullptr, &context);

    if (FAILED(hr)) return false;

    //ビューポート設定
    D3D11_VIEWPORT vp = {};
	vp.Width = static_cast<float>(GetSystemMetrics(SM_CXSCREEN));
	vp.Height = static_cast<float>(GetSystemMetrics(SM_CYSCREEN));
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0.0f;
	vp.TopLeftY = 0.0f;
	context->RSSetViewports(1, &vp);

    //レンダーターゲットビュー生成、設定
    ID3D11Texture2D* backBuffer;
    swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer);
    device->CreateRenderTargetView(backBuffer, nullptr, &rtv);
    backBuffer->Release();

    // ラスタライザステート設定
    D3D11_RASTERIZER_DESC rd;
    ZeroMemory(&rd, sizeof(rd));
    rd.FillMode = D3D11_FILL_SOLID;
    //rd.CullMode = D3D11_CULL_NONE;
    rd.CullMode = D3D11_CULL_BACK; //裏カリング（表表示）
    //rd.CullMode = D3D11_CULL_FRONT; //表カリング（裏表示）すべて時計回りが表

    rd.DepthClipEnable = TRUE;
    rd.MultisampleEnable = FALSE;

    ID3D11RasterizerState* rs;
    device->CreateRasterizerState(&rd, &rs);

    context->RSSetState(rs);

    //定数バッファ生成
    D3D11_BUFFER_DESC bd = {};
    bd.ByteWidth = sizeof(XMMATRIX);
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bd.CPUAccessFlags = 0;
    bd.MiscFlags = 0;
    bd.StructureByteStride = sizeof(float);

    device->CreateBuffer(&bd, NULL, &worldBuffer);
    context->VSSetConstantBuffers(0, 1, &worldBuffer);

    device->CreateBuffer(&bd, NULL, &viewBuffer);
    context->VSSetConstantBuffers(1, 1, &viewBuffer);

    device->CreateBuffer(&bd, NULL, &projectionBuffer);
    context->VSSetConstantBuffers(2, 1, &projectionBuffer);


    //シェーダー初期化
    CreateVertexShader();
    CreatePixelShader();

	context->IASetInputLayout(vertexLayout);
	context->VSSetShader(vertexShader, nullptr, 0);
	context->PSSetShader(pixelShader, nullptr, 0);

    return true;
}

void RendererDX11::BeginFrame() {
    float clearColor[4] = { 0.3f, 0.3f, 0.3f, 1.0f };
    context->OMSetRenderTargets(1, &rtv, nullptr);
    context->ClearRenderTargetView(rtv, clearColor);
}

void RendererDX11::EndFrame() {
    swapChain->Present(1, 0);
}

void RendererDX11::Shutdown() {
    if (rtv) rtv->Release();
    if (swapChain) swapChain->Release();
    if (context) context->Release();
    if (device) device->Release();
    if (worldBuffer) worldBuffer->Release();
    if (viewBuffer)viewBuffer->Release();
    if (projectionBuffer)projectionBuffer->Release();

    if (vertexShader)vertexShader->Release();
    if (vertexLayout) vertexLayout->Release();
    if (pixelShader)pixelShader->Release();
}

ID3D11Device* RendererDX11::GetDevice() { return device; }
ID3D11DeviceContext* RendererDX11::GetContext() { return context; }

void RendererDX11::SetWorldMatrix(XMMATRIX world){
    XMFLOAT4X4 worldf;
    XMStoreFloat4x4(&worldf, XMMatrixTranspose(world));
    context->UpdateSubresource(worldBuffer, 0, NULL, &worldf, 0, 0);
}

void RendererDX11::SetViewMatrix(XMMATRIX view){
    XMFLOAT4X4 viewf;
    XMStoreFloat4x4(&viewf, XMMatrixTranspose(view));
    context->UpdateSubresource(viewBuffer, 0, NULL, &viewf, 0, 0);
}

void RendererDX11::SetProjectionMatrix(XMMATRIX projection){
    XMFLOAT4X4 projectionf;
    XMStoreFloat4x4(&projectionf, XMMatrixTranspose(projection));
    context->UpdateSubresource(projectionBuffer, 0, NULL, &projectionf, 0, 0);
}

void RendererDX11::SetWorldProjection2D(){
    SetWorldMatrix(XMMatrixIdentity());
    SetViewMatrix(XMMatrixIdentity());

    XMMATRIX projection;
    projection = XMMatrixOrthographicOffCenterLH(0.0f, 1280.0f, 720.0f, 0.0f, 0.0f, 1.0f);
    SetProjectionMatrix(projection);

}

void RendererDX11::SetWorldProjection3D()
{//数値は仮
    XMMATRIX worldViewProjection;
	XMMATRIX world = XMMatrixIdentity();
    XMMATRIX view = XMMatrixLookAtLH(
        XMVectorSet(0.0f, 0.0f, -10.0f, 0.0f), // カメラ位置
        XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f), // 注視点
        XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)  // 上方向
	);
    XMMATRIX projection = XMMatrixPerspectiveFovLH(XMConvertToRadians(60.0f), 1280.0f / 720.0f, 0.1f, 1000.0f);
    worldViewProjection = world * view * projection;
    SetWorldMatrix(world);
    SetViewMatrix(view);
	SetProjectionMatrix(projection);
}


void RendererDX11::CreateVertexShader(){
    FILE* file;
    long int fsize;

    file = fopen("shader/UnlitColorVS.cso", "rb");
    assert(file);

    fsize = _filelength(_fileno(file));
    unsigned char* buffer = new unsigned char[fsize];

    fread(buffer, fsize, 1, file);
    fclose(file);
    device->CreateVertexShader(buffer, fsize, NULL, &vertexShader);

    D3D11_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 4 * 3, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 4 * 6, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 4 * 10, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };
    UINT numElements = ARRAYSIZE(layout);

    device->CreateInputLayout(layout, numElements, buffer, fsize, &vertexLayout);

    delete[] buffer;
}

void RendererDX11::CreatePixelShader(){
    FILE* file;
    long int fsize;

    file = fopen("shader/UnlitColorPS.cso", "rb");
    assert(file);

    fsize = _filelength(_fileno(file));
    unsigned char* buffer = new unsigned char[fsize];

    fread(buffer, fsize, 1, file);
    fclose(file);
    device->CreatePixelShader(buffer, fsize, NULL, &pixelShader);


    delete[] buffer;
}

