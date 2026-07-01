#include "Renderer.h"
#include "D3DX12.h"
#include <iostream>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
#include <cassert>
#include <list>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

#include "imgui.h"
#include "Texture.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx12.h"

using Microsoft::WRL::ComPtr;

namespace RendererDX12 {
    unsigned int AllocateSrvIndex();
    void FreeSrvIndex(unsigned int index);

    const int FrameCount = 2;
    ComPtr<ID3D12Device> g_pd3dDevice = nullptr;
    ComPtr<ID3D12DescriptorHeap> g_pd3dRtvDescHeap = nullptr;
    ComPtr<ID3D12DescriptorHeap> g_pd3dDsvDescHeap = nullptr;
    ComPtr<ID3D12DescriptorHeap> g_pd3dSrvDescHeap = nullptr;
    ComPtr<ID3D12CommandQueue> g_pd3dCommandQueue = nullptr;
    ComPtr<ID3D12GraphicsCommandList> g_pd3dCommandList = nullptr;
    ComPtr<ID3D12CommandAllocator> g_commandAllocators[FrameCount];
    ComPtr<IDXGISwapChain3> g_pSwapChain = nullptr;
    ComPtr<ID3D12Resource> g_mainRenderTargetResource[FrameCount];
    ComPtr<ID3D12Resource> g_depthStencilBuffer = nullptr;
    ComPtr<ID3D12Fence> g_fence = nullptr;
    HANDLE g_fenceEvent = nullptr;
    UINT64 g_fenceLastSignaledValue = 0;
    UINT g_frameIndex = 0;
    UINT g_rtvDescriptorSize = 0;
    UINT g_dsvDescriptorSize = 0;
    UINT g_srvDescriptorSize = 0;

    std::list<unsigned int> g_srvDescriptorPool;

    // シーン用レンダーターゲット
    ComPtr<ID3D12Resource> g_sceneRenderTarget = nullptr;
    ComPtr<ID3D12Resource> g_sceneDepthBuffer = nullptr;
    float g_sceneWidth = 1280.0f;
    float g_sceneHeight = 720.0f;

    // ゲーム用レンダーターゲット
    ComPtr<ID3D12Resource> g_gameRenderTarget = nullptr;
    ComPtr<ID3D12Resource> g_gameDepthBuffer = nullptr;
    float g_gameWidth = 1280.0f;
    float g_gameHeight = 720.0f;

    void PrintDebugMessages();

    // 遅延リサイズ用変数
    float g_scenePendingWidth = 0.0f;
    float g_scenePendingHeight = 0.0f;
    bool g_sceneResizePending = false;

    float g_gamePendingWidth = 0.0f;
    float g_gamePendingHeight = 0.0f;
    bool g_gameResizePending = false;

    void RecreateSceneBuffer(float width, float height);
    void RecreateGameBuffer(float width, float height);

    D3D12_CULL_MODE g_currentCullMode = D3D12_CULL_MODE_BACK;
    bool g_currentDepthEnable = true;
    bool g_currentDepthWrite = true;

    D3D12_CULL_MODE GetCullMode() { return g_currentCullMode; }
    bool GetDepthEnable() { return g_currentDepthEnable; }
    bool GetDepthWrite() { return g_currentDepthWrite; }

    void SetDepthEnable(bool enable) { g_currentDepthEnable = enable; g_currentDepthWrite = enable; }
    void SetDepthReadOnly() { g_currentDepthWrite = false; }
    void SetCullModeBack() { g_currentCullMode = D3D12_CULL_MODE_BACK; }
    void SetCullModeFront() { g_currentCullMode = D3D12_CULL_MODE_FRONT; }
    void SetCullModeNone() { g_currentCullMode = D3D12_CULL_MODE_NONE; }

    XMMATRIX g_currentViewMatrix = XMMatrixIdentity();
    XMMATRIX g_currentProjMatrix = XMMatrixIdentity();
    LIGHT g_currentLightData = {
        TRUE, // Enable
        { FALSE, FALSE, FALSE }, // Dummy
        { 0.0f, -1.0f, -1.0f, 0.0f }, // Direction
        { 0.8f, 0.8f, 0.8f, 1.0f }, // Diffuse
        { 0.2f, 0.2f, 0.2f, 1.0f }, // Ambient
        { -5.0f, 10.0f, 5.0f, 0.0f }, // Position
        { 100.0f, 1.5f, 0.0f, 0.0f } // Parameter
    };
    XMFLOAT4 g_currentCameraPos = { 0.0f, 0.0f, 0.0f, 0.0f };
    const TEXTURE* g_defaultTexture = nullptr;

    void SetViewMatrix(XMMATRIX view) { g_currentViewMatrix = view; }
    void SetProjectionMatrix(XMMATRIX projection) { g_currentProjMatrix = projection; }
    void SetLight(LIGHT light) { g_currentLightData = light; }
    void SetCameraPosition(XMFLOAT4 cameraPos) { g_currentCameraPos = cameraPos; }

    void BindShaderConstants() {
        XMMATRIX transposedView = XMMatrixTranspose(g_currentViewMatrix);
        XMMATRIX transposedProj = XMMatrixTranspose(g_currentProjMatrix);
        
        SetConstant(1, &transposedView, sizeof(transposedView));
        SetConstant(2, &transposedProj, sizeof(transposedProj));
        SetConstant(4, &g_currentLightData, sizeof(g_currentLightData));
        SetConstant(5, &g_currentCameraPos, sizeof(g_currentCameraPos));
    }

    // ビューポート/シザー
    D3D12_VIEWPORT g_viewport = {};
    D3D12_RECT g_scissorRect = {};

    ComPtr<ID3D12RootSignature> g_pd3dRootSignature = nullptr;

    static const unsigned int CONSTANT_BUFFER_SIZE = 256;
    static const unsigned int CONSTANT_BUFFER_MAX = 30000;
    ComPtr<ID3D12Resource> g_constantBuffer[FrameCount];
    byte* g_constantBufferPointer[FrameCount] = { nullptr };
    unsigned int g_constantBufferView[FrameCount][CONSTANT_BUFFER_MAX];
    unsigned int g_constantBufferIndex[FrameCount] = { 0 };

    // 記述子ハンドル取得ヘルパー
    D3D12_CPU_DESCRIPTOR_HANDLE GetRtvHandle(int index) {
        D3D12_CPU_DESCRIPTOR_HANDLE handle = g_pd3dRtvDescHeap->GetCPUDescriptorHandleForHeapStart();
        handle.ptr += index * g_rtvDescriptorSize;
        return handle;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE GetDsvHandle(int index) {
        D3D12_CPU_DESCRIPTOR_HANDLE handle = g_pd3dDsvDescHeap->GetCPUDescriptorHandleForHeapStart();
        handle.ptr += index * g_dsvDescriptorSize;
        return handle;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE GetSrvCpuHandle(int index) {
        D3D12_CPU_DESCRIPTOR_HANDLE handle = g_pd3dSrvDescHeap->GetCPUDescriptorHandleForHeapStart();
        handle.ptr += index * g_srvDescriptorSize;
        return handle;
    }

    D3D12_GPU_DESCRIPTOR_HANDLE GetSrvGpuHandle(int index) {
        D3D12_GPU_DESCRIPTOR_HANDLE handle = g_pd3dSrvDescHeap->GetGPUDescriptorHandleForHeapStart();
        handle.ptr += index * g_srvDescriptorSize;
        return handle;
    }

    void WaitForLastSubmittedFrame() {
        const UINT64 fence = g_fenceLastSignaledValue;
        g_pd3dCommandQueue->Signal(g_fence.Get(), fence);
        g_fenceLastSignaledValue++;
        if (g_fence->GetCompletedValue() < fence) {
            g_fence->SetEventOnCompletion(fence, g_fenceEvent);
            DWORD result = WaitForSingleObject(g_fenceEvent, 2000);
            if (result == WAIT_TIMEOUT) {
                HRESULT reason = g_pd3dDevice->GetDeviceRemovedReason();
                FILE* fp = nullptr;
                fopen_s(&fp, "d3d12_log.txt", "a");
                if (fp) {
                    fprintf(fp, "[FATAL] Fence wait timeout! DeviceRemovedReason: 0x%08X\n", reason);
                    fclose(fp);
                }
                PrintDebugMessages();
                assert(false && "GPU hang or device lost detected!");
            }
        }
        g_frameIndex = g_pSwapChain->GetCurrentBackBufferIndex();
    }

    bool Init(HWND hwnd) {
        // ログファイルをクリア
        FILE* fpLog = nullptr;
        errno_t err = fopen_s(&fpLog, "d3d12_log.txt", "w");
        assert(err == 0 && "Failed to open d3d12_log.txt for writing!");
        if (fpLog) {
            fprintf(fpLog, "--- D3D12 Debug Log Started ---\n");
            fclose(fpLog);
        }

#ifdef _DEBUG
        ComPtr<ID3D12Debug> debugController;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
            debugController->EnableDebugLayer();
        }
#endif

        ComPtr<IDXGIFactory4> factory;
        if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))) return false;

        if (FAILED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&g_pd3dDevice)))) return false;

        D3D12_COMMAND_QUEUE_DESC queueDesc = {};
        queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        if (FAILED(g_pd3dDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&g_pd3dCommandQueue)))) return false;

        RECT rect;
        GetClientRect(hwnd, &rect);
        UINT width = rect.right - rect.left;
        UINT height = rect.bottom - rect.top;

        DXGI_SWAP_CHAIN_DESC1 sd = {};
        sd.BufferCount = FrameCount;
        sd.Width = width;
        sd.Height = height;
        sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.SampleDesc.Count = 1;
        sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

        ComPtr<IDXGISwapChain1> swapChain1;
        factory->CreateSwapChainForHwnd(g_pd3dCommandQueue.Get(), hwnd, &sd, nullptr, nullptr, &swapChain1);
        swapChain1.As(&g_pSwapChain);
        g_frameIndex = g_pSwapChain->GetCurrentBackBufferIndex();

        // 記述子サイズ取得
        g_rtvDescriptorSize = g_pd3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        g_dsvDescriptorSize = g_pd3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
        g_srvDescriptorSize = g_pd3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

        // 記述子ヒープ作成
        D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
        rtvHeapDesc.NumDescriptors = 4; // BackBuffer*2 + Scene + Game
        rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        g_pd3dDevice->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&g_pd3dRtvDescHeap));

        D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
        dsvHeapDesc.NumDescriptors = 3; // MainDepth + SceneDepth + GameDepth
        dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        g_pd3dDevice->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&g_pd3dDsvDescHeap));

        D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
        srvHeapDesc.NumDescriptors = 100000;
        srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        g_pd3dDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&g_pd3dSrvDescHeap));

        // RTV・コマンドアロケータ作成
        for (UINT i = 0; i < FrameCount; i++) {
            g_pSwapChain->GetBuffer(i, IID_PPV_ARGS(&g_mainRenderTargetResource[i]));
            g_pd3dDevice->CreateRenderTargetView(g_mainRenderTargetResource[i].Get(), nullptr, GetRtvHandle(i));
            g_pd3dDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&g_commandAllocators[i]));
        }

        // メイン深度ステンシルバッファ作成
        D3D12_RESOURCE_DESC depthDesc = {};
        depthDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        depthDesc.Width = width;
        depthDesc.Height = height;
        depthDesc.DepthOrArraySize = 1;
        depthDesc.MipLevels = 1;
        depthDesc.Format = DXGI_FORMAT_D32_FLOAT;
        depthDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        depthDesc.SampleDesc.Count = 1;
        depthDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

        D3D12_CLEAR_VALUE depthClear = {};
        depthClear.Format = DXGI_FORMAT_D32_FLOAT;
        depthClear.DepthStencil.Depth = 1.0f;

        auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
        g_pd3dDevice->CreateCommittedResource(
            &heapProperties,
            D3D12_HEAP_FLAG_NONE,
            &depthDesc,
            D3D12_RESOURCE_STATE_DEPTH_WRITE,
            &depthClear,
            IID_PPV_ARGS(&g_depthStencilBuffer)
        );
        g_pd3dDevice->CreateDepthStencilView(g_depthStencilBuffer.Get(), nullptr, GetDsvHandle(0));

        // コマンドリスト作成
        g_pd3dDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, g_commandAllocators[0].Get(), nullptr, IID_PPV_ARGS(&g_pd3dCommandList));
        g_pd3dCommandList->Close();

        // フェンス
        g_pd3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&g_fence));
        g_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);

        // ビューポート
        g_viewport.TopLeftX = 0.0f;
        g_viewport.TopLeftY = 0.0f;
        g_viewport.Width = (float)width;
        g_viewport.Height = (float)height;
        g_viewport.MinDepth = 0.0f;
        g_viewport.MaxDepth = 1.0f;

        g_scissorRect.left = 0;
        g_scissorRect.top = 0;
        g_scissorRect.right = width;
        g_scissorRect.bottom = height;

        // SRVプールの初期化
        g_srvDescriptorPool.clear();
        for (unsigned int i = 3; i < 100000; i++) {
            g_srvDescriptorPool.push_back(i);
        }

        // ルートシグネチャ生成
        {
            D3D12_ROOT_PARAMETER rootParameters[14] = {};
            D3D12_DESCRIPTOR_RANGE range[14] = {};

            // 定数バッファ (0〜5)
            for (unsigned int i = 0; i < 6; i++) {
                range[i].NumDescriptors = 1;
                range[i].BaseShaderRegister = i;
                range[i].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
                range[i].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

                rootParameters[i].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
                rootParameters[i].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
                rootParameters[i].DescriptorTable.NumDescriptorRanges = 1;
                rootParameters[i].DescriptorTable.pDescriptorRanges = &range[i];
            }

            // テクスチャ (6〜13)
            for (unsigned int i = 6; i < 14; i++) {
                range[i].NumDescriptors = 1;
                range[i].BaseShaderRegister = i - 6;
                range[i].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
                range[i].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

                rootParameters[i].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
                rootParameters[i].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
                rootParameters[i].DescriptorTable.NumDescriptorRanges = 1;
                rootParameters[i].DescriptorTable.pDescriptorRanges = &range[i];
            }

            // サンプラー (Static Samplers)
            D3D12_STATIC_SAMPLER_DESC samplerDesc[2] = {};
            samplerDesc[0].Filter = D3D12_FILTER_ANISOTROPIC;
            samplerDesc[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
            samplerDesc[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
            samplerDesc[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
            samplerDesc[0].MipLODBias = 0.0f;
            samplerDesc[0].MaxAnisotropy = 4;
            samplerDesc[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
            samplerDesc[0].BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
            samplerDesc[0].MinLOD = 0.0f;
            samplerDesc[0].MaxLOD = D3D12_FLOAT32_MAX;
            samplerDesc[0].ShaderRegister = 0;
            samplerDesc[0].RegisterSpace = 0;
            samplerDesc[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

            samplerDesc[1].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
            samplerDesc[1].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
            samplerDesc[1].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
            samplerDesc[1].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
            samplerDesc[1].MipLODBias = 0.0f;
            samplerDesc[1].MaxAnisotropy = 16;
            samplerDesc[1].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
            samplerDesc[1].BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
            samplerDesc[1].MinLOD = 0.0f;
            samplerDesc[1].MaxLOD = D3D12_FLOAT32_MAX;
            samplerDesc[1].ShaderRegister = 1;
            samplerDesc[1].RegisterSpace = 0;
            samplerDesc[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

            D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
            rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
            rootSignatureDesc.NumParameters = _countof(rootParameters);
            rootSignatureDesc.pParameters = rootParameters;
            rootSignatureDesc.NumStaticSamplers = 2;
            rootSignatureDesc.pStaticSamplers = samplerDesc;

            HRESULT hr;
            ComPtr<ID3DBlob> blob = nullptr;
            hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, nullptr);
            assert(SUCCEEDED(hr));

            hr = g_pd3dDevice->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&g_pd3dRootSignature));
            assert(SUCCEEDED(hr));
        }

        // 汎用定数バッファ生成
        for (int i = 0; i < FrameCount; i++) {
            auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
            auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(CONSTANT_BUFFER_SIZE * CONSTANT_BUFFER_MAX);

            HRESULT hr = g_pd3dDevice->CreateCommittedResource(
                &heapProperties,
                D3D12_HEAP_FLAG_NONE,
                &bufferDesc,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(&g_constantBuffer[i])
            );
            assert(SUCCEEDED(hr));

            hr = g_constantBuffer[i]->Map(0, nullptr, (void**)&g_constantBufferPointer[i]);
            assert(SUCCEEDED(hr));

            for (int j = 0; j < CONSTANT_BUFFER_MAX; j++) {
                unsigned int index = AllocateSrvIndex();

                D3D12_CONSTANT_BUFFER_VIEW_DESC desc = {};
                desc.BufferLocation = g_constantBuffer[i]->GetGPUVirtualAddress() + j * CONSTANT_BUFFER_SIZE;
                desc.SizeInBytes = CONSTANT_BUFFER_SIZE;

                g_pd3dDevice->CreateConstantBufferView(&desc, GetSrvCpuHandle(index));
                g_constantBufferView[i][j] = index;
            }
            g_constantBufferIndex[i] = 0;
        }

        // デフォルトテクスチャのロード
        g_defaultTexture = Texture::Load("asset\\texture\\Space.jpg");

        return true;
    }

    void Shutdown() {
        WaitForLastSubmittedFrame();

        g_sceneRenderTarget.Reset();
        g_sceneDepthBuffer.Reset();
        g_gameRenderTarget.Reset();
        g_gameDepthBuffer.Reset();
        g_depthStencilBuffer.Reset();

        for (UINT i = 0; i < FrameCount; i++) {
            if (g_constantBuffer[i]) {
                g_constantBuffer[i]->Unmap(0, nullptr);
                g_constantBuffer[i].Reset();
            }
            g_mainRenderTargetResource[i].Reset();
            g_commandAllocators[i].Reset();
        }

        g_pd3dRootSignature.Reset();
        g_pd3dCommandList.Reset();
        g_pd3dCommandQueue.Reset();
        g_pSwapChain.Reset();
        g_pd3dRtvDescHeap.Reset();
        g_pd3dDsvDescHeap.Reset();
        g_pd3dSrvDescHeap.Reset();
        g_fence.Reset();

        if (g_fenceEvent) {
            CloseHandle(g_fenceEvent);
            g_fenceEvent = nullptr;
        }
        g_pd3dDevice.Reset();
    }

    ID3D12Device* GetDevice() { return g_pd3dDevice.Get(); }
    ID3D12DescriptorHeap* GetSrvHeap() { return g_pd3dSrvDescHeap.Get(); }
    int GetFrameCount() { return FrameCount; }
    ID3D12GraphicsCommandList* GetCommandList() { return g_pd3dCommandList.Get(); }
    ID3D12CommandQueue* GetCommandQueue() { return g_pd3dCommandQueue.Get(); }
    ID3D12RootSignature* GetRootSignature() { return g_pd3dRootSignature.Get(); }

    ID3D11Device* GetDevice11() { return nullptr; }
    ID3D11DeviceContext* GetContext11() { return nullptr; }

    void BeginGameRender() {
        if (g_sceneResizePending) {
            RecreateSceneBuffer(g_scenePendingWidth, g_scenePendingHeight);
            g_sceneResizePending = false;
        }
        if (g_gameResizePending) {
            RecreateGameBuffer(g_gamePendingWidth, g_gamePendingHeight);
            g_gameResizePending = false;
        }

        g_commandAllocators[g_frameIndex]->Reset();
        g_pd3dCommandList->Reset(g_commandAllocators[g_frameIndex].Get(), nullptr);

        g_constantBufferIndex[g_frameIndex] = 0;

        ID3D12DescriptorHeap* descriptorHeaps[] = { g_pd3dSrvDescHeap.Get() };
        g_pd3dCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
        g_pd3dCommandList->SetGraphicsRootSignature(g_pd3dRootSignature.Get());

        if (g_gameRenderTarget) {
            D3D12_RESOURCE_BARRIER barrier = {};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition.pResource = g_gameRenderTarget.Get();
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
            g_pd3dCommandList->ResourceBarrier(1, &barrier);

            D3D12_CPU_DESCRIPTOR_HANDLE rtv = GetRtvHandle(3);
            D3D12_CPU_DESCRIPTOR_HANDLE dsv = GetDsvHandle(2);
            g_pd3dCommandList->OMSetRenderTargets(1, &rtv, FALSE, &dsv);

            const float clearColor[] = { 0.1f, 0.1f, 0.1f, 1.0f };
            g_pd3dCommandList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
            g_pd3dCommandList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

            D3D12_VIEWPORT vp = { 0.0f, 0.0f, g_gameWidth, g_gameHeight, 0.0f, 1.0f };
            D3D12_RECT sr = { 0, 0, (LONG)g_gameWidth, (LONG)g_gameHeight };
            g_pd3dCommandList->RSSetViewports(1, &vp);
            g_pd3dCommandList->RSSetScissorRects(1, &sr);
        }
    }

    void BeginSceneRender() {
        g_pd3dCommandList->SetGraphicsRootSignature(g_pd3dRootSignature.Get());

        if (g_gameRenderTarget) {
            D3D12_RESOURCE_BARRIER barrier = {};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition.pResource = g_gameRenderTarget.Get();
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
            g_pd3dCommandList->ResourceBarrier(1, &barrier);
        }

        if (g_sceneRenderTarget) {
            D3D12_RESOURCE_BARRIER barrier = {};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition.pResource = g_sceneRenderTarget.Get();
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
            g_pd3dCommandList->ResourceBarrier(1, &barrier);

            D3D12_CPU_DESCRIPTOR_HANDLE rtv = GetRtvHandle(2);
            D3D12_CPU_DESCRIPTOR_HANDLE dsv = GetDsvHandle(1);
            g_pd3dCommandList->OMSetRenderTargets(1, &rtv, FALSE, &dsv);

            const float clearColor[] = { 0.15f, 0.15f, 0.15f, 1.0f };
            g_pd3dCommandList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
            g_pd3dCommandList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

            D3D12_VIEWPORT vp = { 0.0f, 0.0f, g_sceneWidth, g_sceneHeight, 0.0f, 1.0f };
            D3D12_RECT sr = { 0, 0, (LONG)g_sceneWidth, (LONG)g_sceneHeight };
            g_pd3dCommandList->RSSetViewports(1, &vp);
            g_pd3dCommandList->RSSetScissorRects(1, &sr);
        }
    }

    void BeginFrame() {
        g_pd3dCommandList->SetGraphicsRootSignature(g_pd3dRootSignature.Get());

        if (g_sceneRenderTarget) {
            D3D12_RESOURCE_BARRIER barrier = {};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition.pResource = g_sceneRenderTarget.Get();
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
            g_pd3dCommandList->ResourceBarrier(1, &barrier);
        }

        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = g_mainRenderTargetResource[g_frameIndex].Get();
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        g_pd3dCommandList->ResourceBarrier(1, &barrier);

        D3D12_CPU_DESCRIPTOR_HANDLE rtv = GetRtvHandle(g_frameIndex);
        D3D12_CPU_DESCRIPTOR_HANDLE dsv = GetDsvHandle(0);
        g_pd3dCommandList->OMSetRenderTargets(1, &rtv, FALSE, &dsv);

        const float clearColor[] = { 0.2f, 0.2f, 0.2f, 1.0f };
        g_pd3dCommandList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
        g_pd3dCommandList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

        g_pd3dCommandList->RSSetViewports(1, &g_viewport);
        g_pd3dCommandList->RSSetScissorRects(1, &g_scissorRect);
    }

    void EndFrame() {
        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = g_mainRenderTargetResource[g_frameIndex].Get();
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
        g_pd3dCommandList->ResourceBarrier(1, &barrier);

        g_pd3dCommandList->Close();
        ID3D12CommandList* ppCommandLists[] = { g_pd3dCommandList.Get() };
        g_pd3dCommandQueue->ExecuteCommandLists(1, ppCommandLists);

        g_pSwapChain->Present(1, 0);

        WaitForLastSubmittedFrame();

        PrintDebugMessages();
    }

    bool InitSceneRenderTarget(int width, int height) {
        g_sceneWidth = (float)width;
        g_sceneHeight = (float)height;
        return true;
    }

    void* GetSceneSRV() {
        D3D12_GPU_DESCRIPTOR_HANDLE handle = GetSrvGpuHandle(1);
        return (void*)handle.ptr;
    }

    void ResizeSceneBuffer(float width, float height) {
        if (width <= 0 || height <= 0) return;
        if (g_sceneWidth == width && g_sceneHeight == height && g_sceneRenderTarget != nullptr) return;

        g_scenePendingWidth = width;
        g_scenePendingHeight = height;
        g_sceneResizePending = true;
    }

    void RecreateSceneBuffer(float width, float height) {
        g_sceneWidth = width;
        g_sceneHeight = height;

        g_sceneRenderTarget.Reset();
        g_sceneDepthBuffer.Reset();

        D3D12_RESOURCE_DESC desc = {};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        desc.Width = (UINT64)width;
        desc.Height = (UINT)height;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        desc.SampleDesc.Count = 1;
        desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

        D3D12_CLEAR_VALUE clearVal = {};
        clearVal.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        clearVal.Color[0] = 0.15f;
        clearVal.Color[1] = 0.15f;
        clearVal.Color[2] = 0.15f;
        clearVal.Color[3] = 1.0f;

        auto rtHeapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
        g_pd3dDevice->CreateCommittedResource(
            &rtHeapProps,
            D3D12_HEAP_FLAG_NONE,
            &desc,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            &clearVal,
            IID_PPV_ARGS(&g_sceneRenderTarget)
        );

        g_pd3dDevice->CreateRenderTargetView(g_sceneRenderTarget.Get(), nullptr, GetRtvHandle(2));

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Texture2D.MipLevels = 1;
        g_pd3dDevice->CreateShaderResourceView(g_sceneRenderTarget.Get(), &srvDesc, GetSrvCpuHandle(1));

        D3D12_RESOURCE_DESC depthDesc = {};
        depthDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        depthDesc.Width = (UINT64)width;
        depthDesc.Height = (UINT)height;
        depthDesc.DepthOrArraySize = 1;
        depthDesc.MipLevels = 1;
        depthDesc.Format = DXGI_FORMAT_D32_FLOAT;
        depthDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        depthDesc.SampleDesc.Count = 1;
        depthDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

        D3D12_CLEAR_VALUE depthClear = {};
        depthClear.Format = DXGI_FORMAT_D32_FLOAT;
        depthClear.DepthStencil.Depth = 1.0f;

        auto dsHeapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
        g_pd3dDevice->CreateCommittedResource(
            &dsHeapProps,
            D3D12_HEAP_FLAG_NONE,
            &depthDesc,
            D3D12_RESOURCE_STATE_DEPTH_WRITE,
            &depthClear,
            IID_PPV_ARGS(&g_sceneDepthBuffer)
        );
        g_pd3dDevice->CreateDepthStencilView(g_sceneDepthBuffer.Get(), nullptr, GetDsvHandle(1));
    }

    bool InitGameRenderTarget(int width, int height) {
        g_gameWidth = (float)width;
        g_gameHeight = (float)height;
        return true;
    }

    void* GetGameSRV() {
        D3D12_GPU_DESCRIPTOR_HANDLE handle = GetSrvGpuHandle(2);
        return (void*)handle.ptr;
    }

    void ResizeGameBuffer(float width, float height) {
        if (width <= 0 || height <= 0) return;
        if (g_gameWidth == width && g_gameHeight == height && g_gameRenderTarget != nullptr) return;

        g_gamePendingWidth = width;
        g_gamePendingHeight = height;
        g_gameResizePending = true;
    }

    void RecreateGameBuffer(float width, float height) {
        g_gameWidth = width;
        g_gameHeight = height;

        g_gameRenderTarget.Reset();
        g_gameDepthBuffer.Reset();

        D3D12_RESOURCE_DESC desc = {};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        desc.Width = (UINT64)width;
        desc.Height = (UINT)height;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        desc.SampleDesc.Count = 1;
        desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

        D3D12_CLEAR_VALUE clearVal = {};
        clearVal.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        clearVal.Color[0] = 0.1f;
        clearVal.Color[1] = 0.1f;
        clearVal.Color[2] = 0.1f;
        clearVal.Color[3] = 1.0f;

        auto rtHeapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
        g_pd3dDevice->CreateCommittedResource(
            &rtHeapProps,
            D3D12_HEAP_FLAG_NONE,
            &desc,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            &clearVal,
            IID_PPV_ARGS(&g_gameRenderTarget)
        );

        g_pd3dDevice->CreateRenderTargetView(g_gameRenderTarget.Get(), nullptr, GetRtvHandle(3));

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Texture2D.MipLevels = 1;
        g_pd3dDevice->CreateShaderResourceView(g_gameRenderTarget.Get(), &srvDesc, GetSrvCpuHandle(2));

        D3D12_RESOURCE_DESC depthDesc = {};
        depthDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        depthDesc.Width = (UINT64)width;
        depthDesc.Height = (UINT)height;
        depthDesc.DepthOrArraySize = 1;
        depthDesc.MipLevels = 1;
        depthDesc.Format = DXGI_FORMAT_D32_FLOAT;
        depthDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        depthDesc.SampleDesc.Count = 1;
        depthDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

        D3D12_CLEAR_VALUE depthClear = {};
        depthClear.Format = DXGI_FORMAT_D32_FLOAT;
        depthClear.DepthStencil.Depth = 1.0f;

        auto dsHeapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
        g_pd3dDevice->CreateCommittedResource(
            &dsHeapProps,
            D3D12_HEAP_FLAG_NONE,
            &depthDesc,
            D3D12_RESOURCE_STATE_DEPTH_WRITE,
            &depthClear,
            IID_PPV_ARGS(&g_gameDepthBuffer)
        );
        g_pd3dDevice->CreateDepthStencilView(g_gameDepthBuffer.Get(), nullptr, GetDsvHandle(2));
    }

    // SRVアロケータヘルパー
    unsigned int AllocateSrvIndex() {
        assert(!g_srvDescriptorPool.empty() && "SRV Descriptor pool is empty!");
        unsigned int index = g_srvDescriptorPool.front();
        g_srvDescriptorPool.pop_front();
        return index;
    }

    void FreeSrvIndex(unsigned int index) {
        g_srvDescriptorPool.push_back(index);
    }

    unsigned int CreateShaderResourceView(ID3D12Resource* resource) {
        unsigned int index = AllocateSrvIndex();
        
        D3D12_RESOURCE_DESC resDesc = resource->GetDesc();
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = resDesc.Format;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Texture2D.MipLevels = resDesc.MipLevels;

        g_pd3dDevice->CreateShaderResourceView(resource, &srvDesc, GetSrvCpuHandle(index));
        return index;
    }

    void ReleaseShaderResourceView(unsigned int index) {
        FreeSrvIndex(index);
    }

    // バッファ作成ヘルパー
    std::unique_ptr<VERTEX_BUFFER> CreateVertexBuffer(unsigned int stride, unsigned int size) {
        auto vertexBuffer = std::make_unique<VERTEX_BUFFER>();
        
        auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(stride * size);
        
        HRESULT hr = g_pd3dDevice->CreateCommittedResource(
            &heapProperties,
            D3D12_HEAP_FLAG_NONE,
            &bufferDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&vertexBuffer->Resource)
        );
        assert(SUCCEEDED(hr));
        
        vertexBuffer->Stride = stride;
        vertexBuffer->Size = size;
        return vertexBuffer;
    }

    std::unique_ptr<INDEX_BUFFER> CreateIndexBuffer(unsigned int size) {
        auto indexBuffer = std::make_unique<INDEX_BUFFER>();
        
        auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(unsigned int) * size);
        
        HRESULT hr = g_pd3dDevice->CreateCommittedResource(
            &heapProperties,
            D3D12_HEAP_FLAG_NONE,
            &bufferDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&indexBuffer->Resource)
        );
        assert(SUCCEEDED(hr));
        
        indexBuffer->Size = size;
        return indexBuffer;
    }

    void SetConstant(int slot, const void* data, unsigned int size) {
        assert(g_constantBufferIndex[g_frameIndex] < CONSTANT_BUFFER_MAX);
        
        unsigned int offset = g_constantBufferIndex[g_frameIndex] * CONSTANT_BUFFER_SIZE;
        memcpy(g_constantBufferPointer[g_frameIndex] + offset, data, size);
        
        D3D12_GPU_DESCRIPTOR_HANDLE handle = g_pd3dSrvDescHeap->GetGPUDescriptorHandleForHeapStart();
        handle.ptr += g_constantBufferView[g_frameIndex][g_constantBufferIndex[g_frameIndex]] * g_srvDescriptorSize;
        
        g_pd3dCommandList->SetGraphicsRootDescriptorTable((UINT)slot, handle);
        
        g_constantBufferIndex[g_frameIndex]++;
    }

    void SetTexture(int slot, const TEXTURE* texture) {
        if (!texture) {
            texture = g_defaultTexture;
        }
        if (!texture) return;
        D3D12_GPU_DESCRIPTOR_HANDLE handle = g_pd3dSrvDescHeap->GetGPUDescriptorHandleForHeapStart();
        handle.ptr += texture->SRVIndex * g_srvDescriptorSize;
        g_pd3dCommandList->SetGraphicsRootDescriptorTable((UINT)slot, handle);
    }

    void PrintDebugMessages() {
        ComPtr<ID3D12InfoQueue> infoQueue;
        if (SUCCEEDED(g_pd3dDevice.As(&infoQueue))) {
            UINT64 numStored = infoQueue->GetNumStoredMessages();
            if (numStored > 0) {
                FILE* fp = nullptr;
                fopen_s(&fp, "d3d12_log.txt", "a");
                if (fp) {
                    for (UINT64 i = 0; i < numStored; i++) {
                        SIZE_T messageLength = 0;
                        infoQueue->GetMessage(i, nullptr, &messageLength);
                        
                        std::vector<byte> messageBuffer(messageLength);
                        D3D12_MESSAGE* message = (D3D12_MESSAGE*)messageBuffer.data();
                        infoQueue->GetMessage(i, message, &messageLength);
                        
                        fprintf(fp, "[D3D12 %d] %s\n", message->Severity, message->pDescription);
                        OutputDebugStringA(message->pDescription);
                        OutputDebugStringA("\n");
                    }
                    fclose(fp);
                }
                infoQueue->ClearStoredMessages();
            }
        }
    }
}
