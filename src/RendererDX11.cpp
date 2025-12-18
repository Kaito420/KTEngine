//=====================================================================================
// RendererDX11.cpp
// Author:Kaito Aoki
// Date:2025/06/23
//=====================================================================================

#include "RendererDX11.h"
#include <dxgi.h>
#include <io.h>

namespace {
    D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;

    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
    IDXGISwapChain* swapChain = nullptr;
    ID3D11RenderTargetView* rtv = nullptr;
    ID3D11DepthStencilView* dsv = nullptr;

    ID3D11Buffer* worldBuffer = nullptr;
    ID3D11Buffer* viewBuffer = nullptr;
    ID3D11Buffer* projectionBuffer = nullptr;
	ID3D11Buffer* materialBuffer = nullptr;
	ID3D11Buffer* lightBuffer = nullptr;
	ID3D11Buffer* cameraBuffer = nullptr;

    ID3D11VertexShader* vertexShader = nullptr;
    ID3D11InputLayout* vertexLayout = nullptr;
    ID3D11PixelShader* pixelShader = nullptr;

    ID3D11DepthStencilState* depthStateEnable = nullptr;
    ID3D11DepthStencilState* depthStateDisable = nullptr;

    ID3D11BlendState* blendState = nullptr;
    ID3D11BlendState* blendStateATC = nullptr;

    ID3D11RasterizerState* cullModeNone = nullptr;
    ID3D11RasterizerState* cullModeBack = nullptr;
    ID3D11RasterizerState* cullModeFront = nullptr;
}

bool RendererDX11::Init(HWND hwnd) {

    //スワップチェーン生成
    DXGI_SWAP_CHAIN_DESC scd = {};
    scd.BufferCount = 1;
    RECT rect;
    GetClientRect(hwnd, &rect);
    UINT width = rect.right - rect.left;
    UINT height = rect.bottom - rect.top;
    scd.BufferDesc.Width = (float)width;
    scd.BufferDesc.Height = (float)height;
    scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow = hwnd;
    scd.SampleDesc.Count = 1;
    scd.Windowed = TRUE;

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0,
        D3D11_SDK_VERSION, &scd, &swapChain, &device, &featureLevel, &context);

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

    //デプスステンシルバッファ作成
    ID3D11Texture2D* depthStencile{};
    D3D11_TEXTURE2D_DESC textureDesc{};
    textureDesc.Width = scd.BufferDesc.Width;
    textureDesc.Height = scd.BufferDesc.Height;
    textureDesc.MipLevels = 1;
    textureDesc.ArraySize = 1;
    textureDesc.Format = DXGI_FORMAT_D16_UNORM;
    textureDesc.SampleDesc = scd.SampleDesc;
    textureDesc.Usage = D3D11_USAGE_DEFAULT;
    textureDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    textureDesc.CPUAccessFlags = 0;
    textureDesc.MiscFlags = 0;
    device->CreateTexture2D(&textureDesc, NULL, &depthStencile);

    //デプスステンシルビュー作成
    D3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc{};
    depthStencilViewDesc.Format = textureDesc.Format;
    depthStencilViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    depthStencilViewDesc.Flags = 0;
    device->CreateDepthStencilView(depthStencile, &depthStencilViewDesc, &dsv);
    depthStencile->Release();
    context->OMSetRenderTargets(1, &rtv, dsv);

    // ラスタライザステート設定
    D3D11_RASTERIZER_DESC rd;
    ZeroMemory(&rd, sizeof(rd));
    rd.FillMode = D3D11_FILL_SOLID;
    rd.CullMode = D3D11_CULL_NONE;
    //rd.CullMode = D3D11_CULL_BACK; //裏カリング（表表示）
    //rd.CullMode = D3D11_CULL_FRONT; //表カリング（裏表示）すべて時計回りが表

    rd.DepthClipEnable = TRUE;
    rd.MultisampleEnable = FALSE;

    device->CreateRasterizerState(&rd, &cullModeNone);

    rd.CullMode = D3D11_CULL_BACK;

    device->CreateRasterizerState(&rd, &cullModeBack);

    rd.CullMode = D3D11_CULL_FRONT;
    device->CreateRasterizerState(&rd, &cullModeFront);

    context->RSSetState(cullModeBack);

	//ブレンドステート設定
	D3D11_BLEND_DESC blendDesc = {};
	blendDesc.AlphaToCoverageEnable = FALSE;
	blendDesc.IndependentBlendEnable = FALSE;
	blendDesc.RenderTarget[0].BlendEnable = TRUE;
	blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
	blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	device->CreateBlendState(&blendDesc, &blendState);

	blendDesc.AlphaToCoverageEnable = TRUE;
	device->CreateBlendState(&blendDesc, &blendStateATC);

	context->OMSetBlendState(blendState, nullptr, 0xffffffff);
	float blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	context->OMSetBlendState(blendState, blendFactor, 0xffffffff);



    //深度ステンシルステート設定
    D3D11_DEPTH_STENCIL_DESC depthStencilDesc = {};
    depthStencilDesc.DepthEnable = TRUE;
    depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    depthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS;
    depthStencilDesc.StencilEnable = FALSE;
    device->CreateDepthStencilState(&depthStencilDesc, &depthStateEnable);

    depthStencilDesc.DepthEnable = FALSE;
    depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    device->CreateDepthStencilState(&depthStencilDesc, &depthStateDisable);

    //初期有効
    context->OMSetDepthStencilState(depthStateEnable, NULL);

    // サンプラーステート設定
    D3D11_SAMPLER_DESC samplerDesc{};
    samplerDesc.Filter = D3D11_FILTER_ANISOTROPIC;
    samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.MaxAnisotropy = 4;
    samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

    ID3D11SamplerState* samplerState{};
    device->CreateSamplerState(&samplerDesc, &samplerState);

    context->PSSetSamplers(0, 1, &samplerState);

    //定数バッファ生成
    D3D11_BUFFER_DESC bd = {};
    bd.ByteWidth = sizeof(XMFLOAT4X4);
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

    bd.ByteWidth = sizeof(MATERIAL);
	device->CreateBuffer(&bd, NULL, &materialBuffer);
	context->VSSetConstantBuffers(3, 1, &materialBuffer);
	context->PSSetConstantBuffers(3, 1, &materialBuffer);

	bd.ByteWidth = sizeof(LIGHT);
	device->CreateBuffer(&bd, NULL, &lightBuffer);
	context->VSSetConstantBuffers(4, 1, &lightBuffer);
	context->PSSetConstantBuffers(4, 1, &lightBuffer);

    //初期ライト追加
    LIGHT light{};
    light.Enable = true;
    light.Direction = XMFLOAT4(0.0f, 0.0f, 1.0f, 0.0f);
    light.Ambient = XMFLOAT4(0.2f, 0.2f, 0.2f, 1.0f);
    light.Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    SetLight(light);

	bd.ByteWidth = sizeof(XMFLOAT4);
	device->CreateBuffer(&bd, NULL, &cameraBuffer);
	context->VSSetConstantBuffers(5, 1, &cameraBuffer);
	context->PSSetConstantBuffers(5, 1, &cameraBuffer);

    //シェーダー初期化
    CreateVertexShader();
    CreatePixelShader();

	context->IASetInputLayout(vertexLayout);
	context->VSSetShader(vertexShader, nullptr, 0);
	context->PSSetShader(pixelShader, nullptr, 0);

    // マテリアル初期化
    MATERIAL material{};
    material.Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    material.Ambient = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    SetMaterial(material);

    return true;
}

void RendererDX11::BeginFrame() {
    float clearColor[4] = { 0.3f, 0.3f, 0.3f, 1.0f };
    context->ClearRenderTargetView(rtv, clearColor);
    context->ClearDepthStencilView(dsv, D3D11_CLEAR_DEPTH, 1.0f, 0);
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
	if (materialBuffer)materialBuffer->Release();
	if (lightBuffer)lightBuffer->Release();

    if (vertexShader)vertexShader->Release();
    if (vertexLayout) vertexLayout->Release();
    if (pixelShader)pixelShader->Release();

    if (depthStateEnable) depthStateEnable->Release();
    if(depthStateDisable) depthStateDisable->Release();

    if(blendState) blendState->Release();
    if(blendStateATC) blendStateATC->Release();

    if(cullModeNone) cullModeNone->Release();
    if (cullModeBack) cullModeBack->Release();
    if(cullModeFront) cullModeFront->Release();
}

ID3D11Device* RendererDX11::GetDevice() { return device; }
ID3D11DeviceContext* RendererDX11::GetContext() { return context; }

void RendererDX11::SetDepthEnable(bool enable){
    if(enable)
        context->OMSetDepthStencilState(depthStateEnable, NULL);
    else
        context->OMSetDepthStencilState(depthStateDisable, NULL);

}

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

void RendererDX11::SetMaterial(MATERIAL material)
{
    context->UpdateSubresource(materialBuffer, 0, NULL, &material, 0, 0);
}

void RendererDX11::SetLight(LIGHT light){
	context->UpdateSubresource(lightBuffer, 0, NULL, &light, 0, 0);
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

    file = fopen("shader/UnlitTextureVS.cso", "rb");
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

    file = fopen("shader/UnlitTexturePS.cso", "rb");
    assert(file);

    fsize = _filelength(_fileno(file));
    unsigned char* buffer = new unsigned char[fsize];

    fread(buffer, fsize, 1, file);
    fclose(file);
    device->CreatePixelShader(buffer, fsize, NULL, &pixelShader);


    delete[] buffer;
}

void RendererDX11::SetCullModeBack(){
    context->RSSetState(cullModeBack);
}

void RendererDX11::SetCullModeFront(){
    context->RSSetState(cullModeFront);
}

void RendererDX11::SetCullModeNone(){
    context->RSSetState(cullModeNone);
}

