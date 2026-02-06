//=====================================================================================
// RendererDX11.cpp
// Author:Kaito Aoki
// Date:2025/06/23
//=====================================================================================

#include "RendererDX11.h"
#include <dxgi.h>
#include <io.h>
#include "ShaderManager.h"

namespace {
    D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;

    ID3D11Device* _device = nullptr;
    ID3D11DeviceContext* _context = nullptr;
    IDXGISwapChain* _swapChain = nullptr;
    ID3D11RenderTargetView* _rtv = nullptr;
    ID3D11DepthStencilView* _dsv = nullptr;

    ID3D11Buffer* _worldBuffer = nullptr;
    ID3D11Buffer* _viewBuffer = nullptr;
    ID3D11Buffer* _projectionBuffer = nullptr;
	ID3D11Buffer* _materialBuffer = nullptr;
	ID3D11Buffer* _lightBuffer = nullptr;
	ID3D11Buffer* _cameraBuffer = nullptr;

    ID3D11VertexShader* _vertexShader = nullptr;
    ID3D11InputLayout* _vertexLayout = nullptr;
    ID3D11PixelShader* _pixelShader = nullptr;

    ID3D11DepthStencilState* _depthStateEnable = nullptr;
    ID3D11DepthStencilState* _depthStateDisable = nullptr;
	ID3D11DepthStencilState* _depthStateReadOnly = nullptr;

    ID3D11BlendState* _blendState = nullptr;
    ID3D11BlendState* _blendStateATC = nullptr;

    ID3D11RasterizerState* _cullModeNone = nullptr;
    ID3D11RasterizerState* _cullModeBack = nullptr;
    ID3D11RasterizerState* _cullModeFront = nullptr;

    ID3D11Texture2D* _sceneTexture = nullptr;
    ID3D11RenderTargetView* _sceneRTV = nullptr;
    ID3D11ShaderResourceView* _sceneSRV = nullptr;
    ID3D11Texture2D* _sceneDepthTexture = nullptr;
    ID3D11DepthStencilView* _sceneDSV = nullptr;

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
        D3D11_SDK_VERSION, &scd, &_swapChain, &_device, &featureLevel, &_context);

    if (FAILED(hr)) return false;

    //ビューポート設定
    D3D11_VIEWPORT vp = {};
	vp.Width = static_cast<float>(GetSystemMetrics(SM_CXSCREEN));
	vp.Height = static_cast<float>(GetSystemMetrics(SM_CYSCREEN));
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0.0f;
	vp.TopLeftY = 0.0f;
	_context->RSSetViewports(1, &vp);

    //レンダーターゲットビュー生成、設定
    ID3D11Texture2D* backBuffer;
    _swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer);
    _device->CreateRenderTargetView(backBuffer, nullptr, &_rtv);
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
    _device->CreateTexture2D(&textureDesc, NULL, &depthStencile);

    //デプスステンシルビュー作成
    D3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc{};
    depthStencilViewDesc.Format = textureDesc.Format;
    depthStencilViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    depthStencilViewDesc.Flags = 0;
    _device->CreateDepthStencilView(depthStencile, &depthStencilViewDesc, &_dsv);
    depthStencile->Release();
    _context->OMSetRenderTargets(1, &_rtv, _dsv);

    // ラスタライザステート設定
    D3D11_RASTERIZER_DESC rd;
    ZeroMemory(&rd, sizeof(rd));
    rd.FillMode = D3D11_FILL_SOLID;
    rd.CullMode = D3D11_CULL_NONE;
    //rd.CullMode = D3D11_CULL_BACK; //裏カリング（表表示）
    //rd.CullMode = D3D11_CULL_FRONT; //表カリング（裏表示）すべて時計回りが表

    rd.DepthClipEnable = TRUE;
    rd.MultisampleEnable = FALSE;

    _device->CreateRasterizerState(&rd, &_cullModeNone);

    rd.CullMode = D3D11_CULL_BACK;

    _device->CreateRasterizerState(&rd, &_cullModeBack);

    rd.CullMode = D3D11_CULL_FRONT;
    _device->CreateRasterizerState(&rd, &_cullModeFront);

    _context->RSSetState(_cullModeBack);

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
	_device->CreateBlendState(&blendDesc, &_blendState);

	blendDesc.AlphaToCoverageEnable = TRUE;
	_device->CreateBlendState(&blendDesc, &_blendStateATC);

	_context->OMSetBlendState(_blendState, nullptr, 0xffffffff);
	float blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	_context->OMSetBlendState(_blendState, blendFactor, 0xffffffff);



    //深度ステンシルステート設定
    D3D11_DEPTH_STENCIL_DESC depthStencilDesc = {};
    depthStencilDesc.DepthEnable = TRUE;
    depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    depthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS;
    depthStencilDesc.StencilEnable = FALSE;
    _device->CreateDepthStencilState(&depthStencilDesc, &_depthStateEnable);

    depthStencilDesc.DepthEnable = FALSE;
    depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    _device->CreateDepthStencilState(&depthStencilDesc, &_depthStateDisable);

	depthStencilDesc.DepthEnable = TRUE;
	depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	depthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS;
	depthStencilDesc.StencilEnable = FALSE;
	_device->CreateDepthStencilState(&depthStencilDesc, &_depthStateReadOnly);

    //初期有効
    _context->OMSetDepthStencilState(_depthStateEnable, NULL);

    // サンプラーステート設定
    D3D11_SAMPLER_DESC samplerDesc{};
    samplerDesc.Filter = D3D11_FILTER_ANISOTROPIC;
    samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.MaxAnisotropy = 4;
    samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

    ID3D11SamplerState* samplerState{};
    _device->CreateSamplerState(&samplerDesc, &samplerState);

    _context->PSSetSamplers(0, 1, &samplerState);

    //定数バッファ生成
    D3D11_BUFFER_DESC bd = {};
    bd.ByteWidth = sizeof(XMFLOAT4X4);
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bd.CPUAccessFlags = 0;
    bd.MiscFlags = 0;
    bd.StructureByteStride = sizeof(float);

    _device->CreateBuffer(&bd, NULL, &_worldBuffer);
    _context->VSSetConstantBuffers(0, 1, &_worldBuffer);

    _device->CreateBuffer(&bd, NULL, &_viewBuffer);
    _context->VSSetConstantBuffers(1, 1, &_viewBuffer);

    _device->CreateBuffer(&bd, NULL, &_projectionBuffer);
    _context->VSSetConstantBuffers(2, 1, &_projectionBuffer);

    bd.ByteWidth = sizeof(MATERIAL);
	_device->CreateBuffer(&bd, NULL, &_materialBuffer);
	_context->VSSetConstantBuffers(3, 1, &_materialBuffer);
	_context->PSSetConstantBuffers(3, 1, &_materialBuffer);

	bd.ByteWidth = sizeof(LIGHT);
	_device->CreateBuffer(&bd, NULL, &_lightBuffer);
	_context->VSSetConstantBuffers(4, 1, &_lightBuffer);
	_context->PSSetConstantBuffers(4, 1, &_lightBuffer);

    //初期ライト追加
    LIGHT light{};
    light.Enable = true;
    light.Direction = XMFLOAT4(0.0f, -1.0f, -1.0f, 0.0f);
    light.Ambient = XMFLOAT4(0.2f, 0.2f, 0.2f, 1.0f);
    light.Diffuse = XMFLOAT4(0.8f, 0.8f, 0.8f, 1.0f);
	light.Position = XMFLOAT4(-5.0f, 10.0f, 5.0f, 0.0f);
	light.Parameter = XMFLOAT4(100.0f, 1.5f, 0.0f, 0.0f);
    SetLight(light);

	bd.ByteWidth = sizeof(XMFLOAT4);
	_device->CreateBuffer(&bd, NULL, &_cameraBuffer);
	_context->VSSetConstantBuffers(5, 1, &_cameraBuffer);
	_context->PSSetConstantBuffers(5, 1, &_cameraBuffer);

    //デフォルトシェーダー(UnlitTexture)初期化
    CreateVertexShader();
    CreatePixelShader();

    //シェーダー読み込み
    ShaderReload();



    // マテリアル初期化
    MATERIAL material{};
    material.Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    material.Ambient = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    SetMaterial(material);

    return true;
}

void RendererDX11::BeginFrame() {
    float clearColor[4] = { 0.3f, 0.3f, 0.3f, 1.0f };
    _context->OMSetRenderTargets(1, &_rtv, _dsv);
    _context->ClearRenderTargetView(_rtv, clearColor);
    _context->ClearDepthStencilView(_dsv, D3D11_CLEAR_DEPTH, 1.0f, 0);
}

void RendererDX11::EndFrame() {
    _swapChain->Present(1, 0);
}

void RendererDX11::Shutdown() {
    if (_rtv) _rtv->Release();
    if (_swapChain) _swapChain->Release();
    if (_context) _context->Release();
    if (_device) _device->Release();
    if (_worldBuffer) _worldBuffer->Release();
    if (_viewBuffer)_viewBuffer->Release();
    if (_projectionBuffer)_projectionBuffer->Release();
	if (_materialBuffer)_materialBuffer->Release();
	if (_lightBuffer)_lightBuffer->Release();

    if (_vertexShader)_vertexShader->Release();
    if (_vertexLayout) _vertexLayout->Release();
    if (_pixelShader)_pixelShader->Release();

    if (_depthStateEnable) _depthStateEnable->Release();
    if(_depthStateDisable) _depthStateDisable->Release();
	if (_depthStateReadOnly) _depthStateReadOnly->Release();

    if(_blendState) _blendState->Release();
    if(_blendStateATC) _blendStateATC->Release();

    if(_cullModeNone) _cullModeNone->Release();
    if (_cullModeBack) _cullModeBack->Release();
    if(_cullModeFront) _cullModeFront->Release();

    if (_sceneTexture) _sceneTexture->Release();
    if (_sceneRTV) _sceneRTV->Release();
    if (_sceneSRV) _sceneRTV->Release();
    if(_sceneDepthTexture) _sceneDepthTexture->Release();
    if(_sceneDSV)_sceneDSV->Release();
}

ID3D11Device* RendererDX11::GetDevice() { return _device; }
ID3D11DeviceContext* RendererDX11::GetContext() { return _context; }

void RendererDX11::SetDepthEnable(bool enable){
    if(enable)
        _context->OMSetDepthStencilState(_depthStateEnable, NULL);
    else
        _context->OMSetDepthStencilState(_depthStateDisable, NULL);

}

void RendererDX11::SetDepthReadOnly()
{
	_context->OMSetDepthStencilState(_depthStateReadOnly, NULL);
}

void RendererDX11::SetWorldMatrix(XMMATRIX world){
    XMFLOAT4X4 worldf;
    XMStoreFloat4x4(&worldf, XMMatrixTranspose(world));
    _context->UpdateSubresource(_worldBuffer, 0, NULL, &worldf, 0, 0);
}

void RendererDX11::SetViewMatrix(XMMATRIX view){
    XMFLOAT4X4 viewf;
    XMStoreFloat4x4(&viewf, XMMatrixTranspose(view));
    _context->UpdateSubresource(_viewBuffer, 0, NULL, &viewf, 0, 0);
}

void RendererDX11::SetProjectionMatrix(XMMATRIX projection){
    XMFLOAT4X4 projectionf;
    XMStoreFloat4x4(&projectionf, XMMatrixTranspose(projection));
    _context->UpdateSubresource(_projectionBuffer, 0, NULL, &projectionf, 0, 0);
}

void RendererDX11::SetMaterial(MATERIAL material)
{
    _context->UpdateSubresource(_materialBuffer, 0, NULL, &material, 0, 0);
}

void RendererDX11::SetLight(LIGHT light){
	_context->UpdateSubresource(_lightBuffer, 0, NULL, &light, 0, 0);
}

void RendererDX11::SetCameraPosition(XMFLOAT4 cameraPos){
	_context->UpdateSubresource(_cameraBuffer, 0, NULL, &cameraPos, 0, 0);
}

void RendererDX11::SetWorldProjection2D(){
    SetWorldMatrix(XMMatrixIdentity());
    SetViewMatrix(XMMatrixIdentity());

    XMMATRIX projection;
    projection = XMMatrixOrthographicOffCenterLH(0.0f, SCREEN_WIDTH, SCREEN_HEIGHT, 0.0f, 0.0f, 1.0f);
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
    _device->CreateVertexShader(buffer, fsize, NULL, &_vertexShader);

    D3D11_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 4 * 3, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 4 * 6, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 4 * 10, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };
    UINT numElements = ARRAYSIZE(layout);

    _device->CreateInputLayout(layout, numElements, buffer, fsize, &_vertexLayout);

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
    _device->CreatePixelShader(buffer, fsize, NULL, &_pixelShader);


    delete[] buffer;
}

void RendererDX11::CreateVertexShader(ID3D11VertexShader** vertexShader, ID3D11InputLayout** vertexLayout, const char* fileName){
    FILE* file;
    long int fsize;

    file = fopen(fileName, "rb");
    assert(file);

    fsize = _filelength(_fileno(file));
    unsigned char* buffer = new unsigned char[fsize];

    fread(buffer, fsize, 1, file);
    fclose(file);
    _device->CreateVertexShader(buffer, fsize, NULL, vertexShader);

    D3D11_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 4 * 3, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 4 * 6, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 4 * 10, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };
    UINT numElements = ARRAYSIZE(layout);

    _device->CreateInputLayout(layout, numElements, buffer, fsize, vertexLayout);

    delete[] buffer;
}

void RendererDX11::CreatePixelShader(ID3D11PixelShader** pixelShader, const char* fileName){
    FILE* file;
    long int fsize;

    file = fopen(fileName, "rb");
    assert(file);

    fsize = _filelength(_fileno(file));
    unsigned char* buffer = new unsigned char[fsize];

    fread(buffer, fsize, 1, file);
    fclose(file);
    _device->CreatePixelShader(buffer, fsize, NULL, pixelShader);


    delete[] buffer;
}

void RendererDX11::SetCullModeBack(){
    _context->RSSetState(_cullModeBack);
}

void RendererDX11::SetCullModeFront(){
    _context->RSSetState(_cullModeFront);
}

void RendererDX11::SetCullModeNone(){
    _context->RSSetState(_cullModeNone);
}

void RendererDX11::SetDefaultShader(){
    _context->IASetInputLayout(_vertexLayout);
    _context->VSSetShader(_vertexShader, nullptr, 0);
	_context->PSSetShader(_pixelShader, nullptr, 0);
}

void RendererDX11::ShaderReload(){
	ShaderManager::Instance().LoadVertexShader("UnlitTexture", "shader/UnlitTextureVS.cso");
	ShaderManager::Instance().LoadPixelShader("UnlitTexture", "shader/UnlitTexturePS.cso");
	ShaderManager::Instance().LoadVertexShader("DirectionalLight", "shader/VertexDirectionalLightingVS.cso");
	ShaderManager::Instance().LoadPixelShader("DirectionalLight", "shader/VertexDirectionalLightingPS.cso");
	ShaderManager::Instance().LoadVertexShader("Toon", "shader/ToonVS.cso");
	ShaderManager::Instance().LoadPixelShader("Toon", "shader/ToonPS.cso");
}

bool RendererDX11::InitSceneRenderTarget(int width, int height){
    HRESULT hr;

    //テクスチャ作成
    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width = width;
    texDesc.Height = height;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 1;
    texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    texDesc.SampleDesc.Count = 1;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    texDesc.CPUAccessFlags = 0;
    texDesc.MiscFlags = 0;

    hr = _device->CreateTexture2D(&texDesc, NULL, &_sceneTexture);
    if (FAILED(hr)) return false;

    //レンダーターゲットビュー作成
    D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
    rtvDesc.Format = texDesc.Format;
    rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    hr = _device->CreateRenderTargetView(_sceneTexture, &rtvDesc, &_sceneRTV);
    if (FAILED(hr)) return false;

    //シェーダーリソースビュー作成（ImGuiで表示用）
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = texDesc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = 1;
    hr = _device->CreateShaderResourceView(_sceneTexture, &srvDesc, &_sceneSRV);
    if (FAILED(hr)) return false;

    //深度バッファ作成（シーン用）
    //バックバッファ用の深度バッファとは別に用意する必要がある
    D3D11_TEXTURE2D_DESC depthTexDesc = texDesc; //サイズ等は同じ
    depthTexDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; //一般的な深度フォーマット
    depthTexDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

    hr = _device->CreateTexture2D(&depthTexDesc, NULL, &_sceneDepthTexture);
    if (FAILED(hr)) return false;

    D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = depthTexDesc.Format;
    dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    hr = _device->CreateDepthStencilView(_sceneDepthTexture, &dsvDesc, &_sceneDSV);
    if (FAILED(hr)) return false;

    return true;
}

void RendererDX11::BeginSceneRender(){
    float clearColor[4] = { 0.1f, 0.2f, 0.4f, 1.0f }; // シーン背景色（Unityっぽい青）

    //シーン用レンダーターゲットに切り替え
    _context->OMSetRenderTargets(1, &_sceneRTV, _sceneDSV);

    //クリア
    _context->ClearRenderTargetView(_sceneRTV, clearColor);
    _context->ClearDepthStencilView(_sceneDSV, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

    //ビューポートも設定しなおすのが安全
    D3D11_VIEWPORT vp = {};
    vp.Width = (float)1280;  //※InitSceneRenderTargetで指定したサイズと合わせる
    vp.Height = (float)720;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    _context->RSSetViewports(1, &vp);
}

void* RendererDX11::GetSceneSRV(){
    return (void*)_sceneSRV;
}

