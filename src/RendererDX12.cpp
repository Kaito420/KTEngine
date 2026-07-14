#include "Renderer.h"
#include "D3DX12.h"
#include "PostProcessSystem.h"
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
#include "ShaderManager.h"
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

    struct GBufferTarget {
        ComPtr<ID3D12Resource> Resource = nullptr;
        unsigned int RtvIndex = 0;
        unsigned int SrvIndex = 0;
        D3D12_RESOURCE_STATES State = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        
        void Release() {
            Resource.Reset();
            if (SrvIndex != 0) {
                FreeSrvIndex(SrvIndex);
                SrvIndex = 0;
            }
            State = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        }
    };
    
    struct GBufferSet {
        GBufferTarget Color;
        GBufferTarget Normal;
        GBufferTarget Position;
        GBufferTarget Metallic;
        GBufferTarget Specular;
        GBufferTarget Roughness;
        
        void Release() {
            Color.Release();
            Normal.Release();
            Position.Release();
            Metallic.Release();
            Specular.Release();
            Roughness.Release();
        }
    };
    
    GBufferSet g_sceneGBuffer;
    GBufferSet g_gameGBuffer;

    // ポストプロセス用中間バッファ
    struct PostProcessBuffers {
        GBufferTarget BrightTarget;
        GBufferTarget BloomMips[4];
        GBufferTarget BloomBlur[4];

        void Release() {
            BrightTarget.Release();
            for (int i = 0; i < 4; i++) {
                BloomMips[i].Release();
                BloomBlur[i].Release();
            }
        }
    };
    PostProcessBuffers g_scenePostProcess;
    PostProcessBuffers g_gamePostProcess;

    ComPtr<ID3D12Resource> g_fullScreenQuadVB = nullptr;
    D3D12_VERTEX_BUFFER_VIEW g_fullScreenQuadVBView = {};

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

    // シャドウマップ関連のグローバル変数
    ComPtr<ID3D12Resource> g_shadowMapArray;
    unsigned int g_shadowMapDsvIndexStart = 3; //MainDepth(0), SceneDepth(1), GameDepth(2)の次から割り当てる
    unsigned int g_shadowMapSrvIndex = 0; // プールから取得したSRVインデックスを保持
    ComPtr<ID3D12PipelineState> g_shadowPipelineState;

    void PrintDebugMessages();

    D3D12_RESOURCE_STATES g_sceneRTState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    D3D12_RESOURCE_STATES g_gameRTState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

    void TransitionTarget(GBufferTarget& target, D3D12_RESOURCE_STATES targetState) {
        if (!target.Resource) return;
        if (target.State == targetState) return;

        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = target.Resource.Get();
        barrier.Transition.StateBefore = target.State;
        barrier.Transition.StateAfter = targetState;
        g_pd3dCommandList->ResourceBarrier(1, &barrier);

        target.State = targetState;

        if (target.Resource == g_sceneRenderTarget) {
            g_sceneRTState = targetState;
        } else if (target.Resource == g_gameRenderTarget) {
            g_gameRTState = targetState;
        }
    }

    // シーン/ゲーム RT を一時的に GBufferTarget として参照するヘルパー
    GBufferTarget g_tempSceneRT;
    GBufferTarget g_tempGameRT;

    void UpdateTempRTReference() {
        // Scene RT: SRV index = 1, RTV index = 2
        g_tempSceneRT.Resource = g_sceneRenderTarget;
        g_tempSceneRT.SrvIndex = 1;
        g_tempSceneRT.RtvIndex = 2;
        g_tempSceneRT.State = g_sceneRTState;

        // Game RT: SRV index = 2, RTV index = 3
        g_tempGameRT.Resource = g_gameRenderTarget;
        g_tempGameRT.SrvIndex = 2;
        g_tempGameRT.RtvIndex = 3;
        g_tempGameRT.State = g_gameRTState;
    }

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
        0,    // DiffuseModel (Lambert)
        0,    // ShadingModel (Smooth)
        1,    // SpecularModel (Phong)
        { 0.0f, -1.0f, -1.0f, 0.0f }, // Direction
        { 0.8f, 0.8f, 0.8f, 1.0f }, // Diffuse
        { 0.2f, 0.2f, 0.2f, 1.0f }, // Ambient
        { -5.0f, 10.0f, 5.0f, 0.0f }, // Position
        { 100.0f, 1.5f, 0.0f, 0.0f }, // Parameter
        { 1.0f, 1.0f, 1.0f, 1.0f }, // RimColor
        2.0f, // RimPower
        0,    // RimLightModel (Off)
        1.5f, // Intensity (光量ブースト)
        0.4f, // AmbientIntensity (環境光強度)
        1.0f, // Exposure (露出)
        { 0.0f, 0.0f, 0.0f } // DummyLight
    };
    XMFLOAT4 g_currentCameraPos = { 0.0f, 0.0f, 0.0f, 0.0f };
    const TEXTURE* g_defaultTexture = nullptr;
    
    bool g_isGeometryPass = false;
    bool IsGeometryPass() { return g_isGeometryPass; }
    void SetGeometryPass(bool active) { g_isGeometryPass = active; }

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
        rtvHeapDesc.NumDescriptors = 64; // BackBuffer*2 + Scene + Game + GBuffers*12 + PostProcess buffers
        rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        g_pd3dDevice->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&g_pd3dRtvDescHeap));

        D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
        dsvHeapDesc.NumDescriptors = 4; // MainDepth + SceneDepth + GameDepth + ShadowMap(1) (ライトを増やす場合はここのサイズも増やす)
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
            D3D12_STATIC_SAMPLER_DESC samplerDesc[3] = {};
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

            // シャドウサンプラー（register s2）の定義
            samplerDesc[2].Filter = D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
            samplerDesc[2].AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
            samplerDesc[2].AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
            samplerDesc[2].AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
            samplerDesc[2].MipLODBias = 0.0f;
            samplerDesc[2].MaxAnisotropy = 1;
            samplerDesc[2].ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL; // 深度比較
            samplerDesc[2].BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE; // 影響範囲外は光が当たっている扱い
            samplerDesc[2].MinLOD = 0.0f;
            samplerDesc[2].MaxLOD = D3D12_FLOAT32_MAX;
            samplerDesc[2].ShaderRegister = 2; // s2 に割り当て
            samplerDesc[2].RegisterSpace = 0;
            samplerDesc[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

            D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
            rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
            rootSignatureDesc.NumParameters = _countof(rootParameters);
            rootSignatureDesc.pParameters = rootParameters;
            rootSignatureDesc.NumStaticSamplers = _countof(samplerDesc);
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

        // 各種標準シェーダーの事前ロード
        ShaderManager::Instance().LoadVertexShader("UnlitTexture", "shader/UnlitTextureVS.cso");
        ShaderManager::Instance().LoadPixelShader("UnlitTexture", "shader/UnlitTexturePS.cso");

        ShaderManager::Instance().LoadVertexShader("DirectionalLight", "shader/VertexDirectionalLightingVS.cso");
        ShaderManager::Instance().LoadPixelShader("DirectionalLight", "shader/VertexDirectionalLightingPS.cso");

        ShaderManager::Instance().LoadVertexShader("Toon", "shader/ToonVS.cso");
        ShaderManager::Instance().LoadPixelShader("Toon", "shader/ToonPS.cso");

        ShaderManager::Instance().LoadVertexShader("UnlitColor", "shader/UnlitColorVS.cso");
        ShaderManager::Instance().LoadPixelShader("UnlitColor", "shader/UnlitColorPS.cso");

        ShaderManager::Instance().LoadVertexShader("Geometry", "shader/GeometryVS.cso");
        ShaderManager::Instance().LoadPixelShader("Geometry", "shader/GeometryPS.cso");

        ShaderManager::Instance().LoadVertexShader("Deferred", "shader/DeferredVS.cso");
        ShaderManager::Instance().LoadPixelShader("Deferred", "shader/DeferredPS.cso");

        // Bloom ポストプロセスシェーダーのプリロード
        ShaderManager::Instance().LoadPixelShader("BloomBright", "shader/BloomBrightPS.cso");
        ShaderManager::Instance().LoadPixelShader("BloomBlur", "shader/BloomBlurPS.cso");
        ShaderManager::Instance().LoadPixelShader("BloomDownsample", "shader/BloomDownsamplePS.cso");
        ShaderManager::Instance().LoadPixelShader("BloomComposite", "shader/BloomCompositePS.cso");

        // Color Grading　ポストプロセスシェーダーのプリロード
        ShaderManager::Instance().LoadPixelShader("ColorGrading", "shader/ColorGradingPS.cso");

        // Depth of Field ポストプロセスシェーダーのプリロード
        ShaderManager::Instance().LoadPixelShader("DepthOfField", "shader/DepthOfFieldPS.cso");

        // 全画面矩形用頂点データの作成
        {
            Vertex vertices[] = {
                { XMFLOAT3(-1.0f,  1.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, -1.0f), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), XMFLOAT2(0.0f, 0.0f) },
                { XMFLOAT3( 1.0f,  1.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, -1.0f), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), XMFLOAT2(1.0f, 0.0f) },
                { XMFLOAT3(-1.0f, -1.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, -1.0f), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), XMFLOAT2(0.0f, 1.0f) },
                
                { XMFLOAT3(-1.0f, -1.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, -1.0f), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), XMFLOAT2(0.0f, 1.0f) },
                { XMFLOAT3( 1.0f,  1.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, -1.0f), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), XMFLOAT2(1.0f, 0.0f) },
                { XMFLOAT3( 1.0f, -1.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, -1.0f), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), XMFLOAT2(1.0f, 1.0f) },
            };

            auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
            auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(vertices));

            HRESULT hr = g_pd3dDevice->CreateCommittedResource(
                &heapProperties,
                D3D12_HEAP_FLAG_NONE,
                &bufferDesc,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(&g_fullScreenQuadVB)
            );
            assert(SUCCEEDED(hr));

            void* pData = nullptr;
            hr = g_fullScreenQuadVB->Map(0, nullptr, &pData);
            assert(SUCCEEDED(hr));
            memcpy(pData, vertices, sizeof(vertices));
            g_fullScreenQuadVB->Unmap(0, nullptr);

            g_fullScreenQuadVBView.BufferLocation = g_fullScreenQuadVB->GetGPUVirtualAddress();
            g_fullScreenQuadVBView.SizeInBytes = sizeof(vertices);
            g_fullScreenQuadVBView.StrideInBytes = sizeof(Vertex);
        }

        // シャドウマップ（Texture2DArray）リソースと各種ビューの生成
        {
            D3D12_RESOURCE_DESC shadowDesc = {};
            shadowDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
            shadowDesc.Width = 2048;
            shadowDesc.Height = 2048;
            shadowDesc.DepthOrArraySize = 1; // ライトが増える場合はここを増やす
            shadowDesc.MipLevels = 1;
            shadowDesc.Format = DXGI_FORMAT_R32_TYPELESS; // DSV(D32)とSRV(R32)で解釈を変えるためTYPELESSを使用
            shadowDesc.SampleDesc.Count = 1;
            shadowDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
            shadowDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

            D3D12_CLEAR_VALUE depthClear = {};
            depthClear.Format = DXGI_FORMAT_D32_FLOAT;
            depthClear.DepthStencil.Depth = 1.0f;

            auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
            HRESULT hr = g_pd3dDevice->CreateCommittedResource(
                &heapProperties,
                D3D12_HEAP_FLAG_NONE,
                &shadowDesc,
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, // 初期状態はシェーダー読み込み用にしておく
                &depthClear,
                IID_PPV_ARGS(&g_shadowMapArray)
            );
            assert(SUCCEEDED(hr));

            // DSVの作成
            D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
            dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
            dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
            dsvDesc.Texture2DArray.MipSlice = 0;
            dsvDesc.Texture2DArray.FirstArraySlice = 0; // ライト0用（スライス0番）
            dsvDesc.Texture2DArray.ArraySize = 1; // 1回で書き込むスライス枚数

            // DSV記述子ヒープの4番目（インデックス3）に書き込む
            g_pd3dDevice->CreateDepthStencilView(g_shadowMapArray.Get(), 
                &dsvDesc, GetDsvHandle(g_shadowMapDsvIndexStart));

            // SRVの生成
            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
            srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
            srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srvDesc.Texture2DArray.MipLevels = 1;
            srvDesc.Texture2DArray.MostDetailedMip = 0;
            srvDesc.Texture2DArray.FirstArraySlice = 0;
            srvDesc.Texture2DArray.ArraySize = 1; // 将来的に複数ライトにする時はここを最大数にする
            
            // SRVプールから空きインデックスを一つ取得し、ビューを登録する
            g_shadowMapSrvIndex = g_srvDescriptorPool.front();
            g_srvDescriptorPool.pop_front();
            g_pd3dDevice->CreateShaderResourceView(g_shadowMapArray.Get(), 
                &srvDesc, GetSrvCpuHandle(g_shadowMapSrvIndex));

        }

        // シャドウマップ用パイプラインステートの作成
        {
            // 頂点シェーダーのロードとバイナリ取得
            ShaderManager::Instance().LoadVertexShader("ShadowVS", "shader/ShadowVS.cso");
            const auto& vsBin = ShaderManager::Instance().GetVertexShaderBinary("ShadowVS");
            D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
            psoDesc.pRootSignature = g_pd3dRootSignature.Get();
            psoDesc.VS.pShaderBytecode = vsBin.data();
            psoDesc.VS.BytecodeLength = vsBin.size();
            psoDesc.PS.pShaderBytecode = nullptr; // ピクセルシェーダーなし
            psoDesc.PS.BytecodeLength = 0;
            // 入力レイアウト
            static const D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
                { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
                { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
                { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
                { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 40, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
            };
            psoDesc.InputLayout.pInputElementDescs = inputElementDescs;
            psoDesc.InputLayout.NumElements = _countof(inputElementDescs);
            psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = 0; // カラー出力なし
            psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
            psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
            psoDesc.RasterizerState.FrontCounterClockwise = FALSE;
            // シャドウアクネ（ジャギー・ノイズ）対策のバイアス値
            psoDesc.RasterizerState.DepthBias = 10000;
            psoDesc.RasterizerState.DepthBiasClamp = 0.0f;
            psoDesc.RasterizerState.SlopeScaledDepthBias = 1.0f;
            psoDesc.RasterizerState.DepthClipEnable = TRUE;
            psoDesc.DepthStencilState.DepthEnable = TRUE;
            psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
            psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
            psoDesc.DepthStencilState.StencilEnable = FALSE;
            psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
            psoDesc.NumRenderTargets = 0; // カラーバッファなし
            psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
            psoDesc.SampleDesc.Count = 1;
            psoDesc.SampleMask = UINT_MAX;
            HRESULT hr = g_pd3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&g_shadowPipelineState));
            assert(SUCCEEDED(hr));
        }

        // デフォルトテクスチャのロード
        g_defaultTexture = Texture::Load("asset\\texture\\default.png");

        return true;
    }

    void Shutdown() {
        WaitForLastSubmittedFrame();

        g_fullScreenQuadVB.Reset();

        g_sceneGBuffer.Release();
        g_gameGBuffer.Release();

        g_scenePostProcess.Release();
        g_gamePostProcess.Release();

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

    enum class CurrentRenderContext {
        None,
        Game,
        Scene
    };
    CurrentRenderContext g_currentRenderContext = CurrentRenderContext::None;

    void BeginGameRender() {
        g_currentRenderContext = CurrentRenderContext::Game;

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
            if (g_isGeometryPass) {
                // MRT ジオメトリパス
                GBufferTarget* targets[6] = {
                    &g_gameGBuffer.Color,
                    &g_gameGBuffer.Normal,
                    &g_gameGBuffer.Position,
                    &g_gameGBuffer.Metallic,
                    &g_gameGBuffer.Specular,
                    &g_gameGBuffer.Roughness
                };
                for (int i = 0; i < 6; i++) {
                    TransitionTarget(*targets[i], D3D12_RESOURCE_STATE_RENDER_TARGET);
                }

                D3D12_CPU_DESCRIPTOR_HANDLE rtvs[6];
                for (int i = 0; i < 6; i++) {
                    rtvs[i] = GetRtvHandle(10 + i);
                }
                D3D12_CPU_DESCRIPTOR_HANDLE dsv = GetDsvHandle(2);
                g_pd3dCommandList->OMSetRenderTargets(6, rtvs, FALSE, &dsv);

                const float clearColors[6][4] = {
                    { 0.1f, 0.1f, 0.1f, 1.0f },
                    { 0.0f, 0.0f, 0.0f, 1.0f },
                    { 0.0f, 0.0f, 0.0f, 1.0f },
                    { 0.0f, 0.0f, 0.0f, 1.0f },
                    { 0.5f, 0.5f, 0.5f, 1.0f },
                    { 0.5f, 0.5f, 0.5f, 1.0f }
                };
                for (int i = 0; i < 6; i++) {
                    g_pd3dCommandList->ClearRenderTargetView(rtvs[i], clearColors[i], 0, nullptr);
                }
                g_pd3dCommandList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
            }
            else {
                // 通常フォワードパス
                GBufferTarget tempRT;
                tempRT.Resource = g_gameRenderTarget;
                tempRT.RtvIndex = 3;
                tempRT.State = g_gameRTState;
                TransitionTarget(tempRT, D3D12_RESOURCE_STATE_RENDER_TARGET);

                D3D12_CPU_DESCRIPTOR_HANDLE rtv = GetRtvHandle(3);
                D3D12_CPU_DESCRIPTOR_HANDLE dsv = GetDsvHandle(2);
                g_pd3dCommandList->OMSetRenderTargets(1, &rtv, FALSE, &dsv);
            }

            D3D12_VIEWPORT vp = { 0.0f, 0.0f, g_gameWidth, g_gameHeight, 0.0f, 1.0f };
            D3D12_RECT sr = { 0, 0, (LONG)g_gameWidth, (LONG)g_gameHeight };
            g_pd3dCommandList->RSSetViewports(1, &vp);
            g_pd3dCommandList->RSSetScissorRects(1, &sr);
        }
    }

    void BeginSceneRender() {
        g_currentRenderContext = CurrentRenderContext::Scene;

        g_pd3dCommandList->SetGraphicsRootSignature(g_pd3dRootSignature.Get());

        if (g_gameRenderTarget) {
            GBufferTarget tempRT;
            tempRT.Resource = g_gameRenderTarget;
            tempRT.RtvIndex = 3;
            tempRT.State = g_gameRTState;
            TransitionTarget(tempRT, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        }

        if (g_sceneRenderTarget) {
            if (g_isGeometryPass) {
                // MRT ジオメトリパス
                GBufferTarget* targets[6] = {
                    &g_sceneGBuffer.Color,
                    &g_sceneGBuffer.Normal,
                    &g_sceneGBuffer.Position,
                    &g_sceneGBuffer.Metallic,
                    &g_sceneGBuffer.Specular,
                    &g_sceneGBuffer.Roughness
                };
                for (int i = 0; i < 6; i++) {
                    TransitionTarget(*targets[i], D3D12_RESOURCE_STATE_RENDER_TARGET);
                }

                D3D12_CPU_DESCRIPTOR_HANDLE rtvs[6];
                for (int i = 0; i < 6; i++) {
                    rtvs[i] = GetRtvHandle(4 + i);
                }
                D3D12_CPU_DESCRIPTOR_HANDLE dsv = GetDsvHandle(1);
                g_pd3dCommandList->OMSetRenderTargets(6, rtvs, FALSE, &dsv);

                const float clearColors[6][4] = {
                    { 0.15f, 0.15f, 0.15f, 1.0f },
                    { 0.0f, 0.0f, 0.0f, 1.0f },
                    { 0.0f, 0.0f, 0.0f, 1.0f },
                    { 0.0f, 0.0f, 0.0f, 1.0f },
                    { 0.5f, 0.5f, 0.5f, 1.0f },
                    { 0.5f, 0.5f, 0.5f, 1.0f }
                };
                for (int i = 0; i < 6; i++) {
                    g_pd3dCommandList->ClearRenderTargetView(rtvs[i], clearColors[i], 0, nullptr);
                }
                g_pd3dCommandList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
            }
            else {
                // 通常フォワードパス
                GBufferTarget tempRT;
                tempRT.Resource = g_sceneRenderTarget;
                tempRT.RtvIndex = 2;
                tempRT.State = g_sceneRTState;
                TransitionTarget(tempRT, D3D12_RESOURCE_STATE_RENDER_TARGET);

                D3D12_CPU_DESCRIPTOR_HANDLE rtv = GetRtvHandle(2);
                D3D12_CPU_DESCRIPTOR_HANDLE dsv = GetDsvHandle(1);
                g_pd3dCommandList->OMSetRenderTargets(1, &rtv, FALSE, &dsv);
            }

            D3D12_VIEWPORT vp = { 0.0f, 0.0f, g_sceneWidth, g_sceneHeight, 0.0f, 1.0f };
            D3D12_RECT sr = { 0, 0, (LONG)g_sceneWidth, (LONG)g_sceneHeight };
            g_pd3dCommandList->RSSetViewports(1, &vp);
            g_pd3dCommandList->RSSetScissorRects(1, &sr);
        }
    }

    void BeginFrame() {
        g_pd3dCommandList->SetGraphicsRootSignature(g_pd3dRootSignature.Get());

        if (g_sceneRenderTarget) {
            GBufferTarget tempRT;
            tempRT.Resource = g_sceneRenderTarget;
            tempRT.RtvIndex = 2;
            tempRT.State = g_sceneRTState;
            TransitionTarget(tempRT, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
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

    void CreateGBufferResource(GBufferTarget& target, float width, float height, DXGI_FORMAT format, unsigned int rtvIndex, XMFLOAT4 clearColor) {
        target.Release();
        target.RtvIndex = rtvIndex;
        target.SrvIndex = AllocateSrvIndex();
        
        D3D12_RESOURCE_DESC desc = {};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        desc.Width = (UINT64)width;
        desc.Height = (UINT)height;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;
        desc.Format = format;
        desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        desc.SampleDesc.Count = 1;
        desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

        D3D12_CLEAR_VALUE clearVal = {};
        clearVal.Format = format;
        clearVal.Color[0] = clearColor.x;
        clearVal.Color[1] = clearColor.y;
        clearVal.Color[2] = clearColor.z;
        clearVal.Color[3] = clearColor.w;

        auto rtHeapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
        g_pd3dDevice->CreateCommittedResource(
            &rtHeapProps,
            D3D12_HEAP_FLAG_NONE,
            &desc,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            &clearVal,
            IID_PPV_ARGS(&target.Resource)
        );

        g_pd3dDevice->CreateRenderTargetView(target.Resource.Get(), nullptr, GetRtvHandle(target.RtvIndex));

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = format;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Texture2D.MipLevels = 1;
        g_pd3dDevice->CreateShaderResourceView(target.Resource.Get(), &srvDesc, GetSrvCpuHandle(target.SrvIndex));
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

        // Scene G-Bufferの作成
        CreateGBufferResource(g_sceneGBuffer.Color, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, 4, XMFLOAT4(0.15f, 0.15f, 0.15f, 1.0f));
        CreateGBufferResource(g_sceneGBuffer.Normal, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT, 5, XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f));
        CreateGBufferResource(g_sceneGBuffer.Position, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT, 6, XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f));
        CreateGBufferResource(g_sceneGBuffer.Metallic, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, 7, XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f));
        CreateGBufferResource(g_sceneGBuffer.Specular, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, 8, XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f));
        CreateGBufferResource(g_sceneGBuffer.Roughness, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, 9, XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f));

        // Scene ポストプロセスバッファの作成
        g_scenePostProcess.Release();
        // BrightTarget (RTV 16)
        CreateGBufferResource(g_scenePostProcess.BrightTarget, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, 16, XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f));
        // BloomMips[0..3] (RTV 17-20) ダウンサンプル
        for (int i = 0; i < 4; i++) {
            float mipW = width / (float)(2 << i);   // 1/2, 1/4, 1/8, 1/16
            float mipH = height / (float)(2 << i);
            if (mipW < 1.0f) mipW = 1.0f;
            if (mipH < 1.0f) mipH = 1.0f;
            CreateGBufferResource(g_scenePostProcess.BloomMips[i], mipW, mipH, DXGI_FORMAT_R8G8B8A8_UNORM, 17 + i, XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f));
        }
        // BloomBlur[0..3] (RTV 21-24) ブラー結果
        for (int i = 0; i < 4; i++) {
            float mipW = width / (float)(2 << i);
            float mipH = height / (float)(2 << i);
            if (mipW < 1.0f) mipW = 1.0f;
            if (mipH < 1.0f) mipH = 1.0f;
            CreateGBufferResource(g_scenePostProcess.BloomBlur[i], mipW, mipH, DXGI_FORMAT_R8G8B8A8_UNORM, 21 + i, XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f));
        }
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

        // Game G-Bufferの作成
        CreateGBufferResource(g_gameGBuffer.Color, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, 10, XMFLOAT4(0.1f, 0.1f, 0.1f, 1.0f));
        CreateGBufferResource(g_gameGBuffer.Normal, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT, 11, XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f));
        CreateGBufferResource(g_gameGBuffer.Position, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT, 12, XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f));
        CreateGBufferResource(g_gameGBuffer.Metallic, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, 13, XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f));
        CreateGBufferResource(g_gameGBuffer.Specular, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, 14, XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f));
        CreateGBufferResource(g_gameGBuffer.Roughness, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, 15, XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f));

        // Game ポストプロセスバッファの作成
        g_gamePostProcess.Release();
        // BrightTarget (RTV 25)
        CreateGBufferResource(g_gamePostProcess.BrightTarget, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, 25, XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f));
        // BloomMips[0..3] (RTV 26-29)
        for (int i = 0; i < 4; i++) {
            float mipW = width / (float)(2 << i);
            float mipH = height / (float)(2 << i);
            if (mipW < 1.0f) mipW = 1.0f;
            if (mipH < 1.0f) mipH = 1.0f;
            CreateGBufferResource(g_gamePostProcess.BloomMips[i], mipW, mipH, DXGI_FORMAT_R8G8B8A8_UNORM, 26 + i, XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f));
        }
        // BloomBlur[0..3] (RTV 30-33)
        for (int i = 0; i < 4; i++) {
            float mipW = width / (float)(2 << i);
            float mipH = height / (float)(2 << i);
            if (mipW < 1.0f) mipW = 1.0f;
            if (mipH < 1.0f) mipH = 1.0f;
            CreateGBufferResource(g_gamePostProcess.BloomBlur[i], mipW, mipH, DXGI_FORMAT_R8G8B8A8_UNORM, 30 + i, XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f));
        }
    }

    void* GetShadowMapSRV(){
        if(!g_shadowMapArray) return nullptr;
        D3D12_GPU_DESCRIPTOR_HANDLE handle = GetSrvGpuHandle(g_shadowMapSrvIndex);
        return (void*)handle.ptr;
    }

    static bool g_isShadowPass = false;
    bool IsShadowPass() { return g_isShadowPass; }
    void SetShadowPass(bool enable) { g_isShadowPass = enable; }
    ID3D12PipelineState* GetShadowPipelineState() { return g_shadowPipelineState.Get(); }
    void BeginShadowRender() {
        // ルートシグネチャをバインド（描画時の定数バッファバインドが正しく機能するようにする）
        g_pd3dCommandList->SetGraphicsRootSignature(g_pd3dRootSignature.Get());

        // 1. ライトのView-Projection行列を計算
        XMVECTOR lightDir = XMLoadFloat4(&g_currentLightData.Direction);
        // 方向ベクトルを正規化（ドラッグ操作などで大きさが増加してもカメラ距離が一定になるようにする）
        float lenSq = XMVector3LengthSq(lightDir).m128_f32[0];
        if (lenSq > 0.0001f) {
            lightDir = XMVector3Normalize(lightDir);
        } else {
            lightDir = XMVectorSet(0.0f, -1.0f, -1.0f, 0.0f);
        }

        // ライトのPositionを参照し、さらにライト後方のオブジェクトがクリップされるのを防ぐため、光線と逆方向に30ユニット押し戻した位置を起点とする
        XMVECTOR lightPos = XMLoadFloat4(&g_currentLightData.Position) - lightDir * 30.0f;
        XMVECTOR target = lightPos + lightDir;
        XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
        if (fabs(XMVectorGetY(lightDir)) > 0.99f) {
            up = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
        }
    
        XMMATRIX lightView = XMMatrixLookAtLH(lightPos, target, up);
        XMMATRIX lightProj = XMMatrixOrthographicLH(40.0f, 40.0f, 0.1f, 100.0f); // 平行光源用 orthographic
        XMMATRIX lightVP = lightView * lightProj;
    
        // HLSL用に転置して定数バッファの変数にセット
        XMStoreFloat4x4(&g_currentLightData.LightVP, XMMatrixTranspose(lightVP));
        // 2. シャドウリソースを DEPTH_WRITE へバリア遷移
        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = g_shadowMapArray.Get();
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_DEPTH_WRITE;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        g_pd3dCommandList->ResourceBarrier(1, &barrier);
        // 3. レンダーターゲットのセット (カラーはNULL、DSVのみバインド)
        D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = GetDsvHandle(g_shadowMapDsvIndexStart);
        g_pd3dCommandList->OMSetRenderTargets(0, nullptr, FALSE, &dsvHandle);
        // 4. クリア
        g_pd3dCommandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
        // 5. ビューポートをシャドウマップサイズ（2048x2048）に設定
        D3D12_VIEWPORT viewport = { 0.0f, 0.0f, 2048.0f, 2048.0f, 0.0f, 1.0f };
        D3D12_RECT scissorRect = { 0, 0, 2048, 2048 };
        g_pd3dCommandList->RSSetViewports(1, &viewport);
        g_pd3dCommandList->RSSetScissorRects(1, &scissorRect);
    }
    void EndShadowRender() {
        // シャドウリソースを PIXEL_SHADER_RESOURCE へ戻す
        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = g_shadowMapArray.Get();
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_DEPTH_WRITE;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        g_pd3dCommandList->ResourceBarrier(1, &barrier);
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

    void DrawFullScreenQuad() {
        g_pd3dCommandList->IASetVertexBuffers(0, 1, &g_fullScreenQuadVBView);
        g_pd3dCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        g_pd3dCommandList->DrawInstanced(6, 1, 0, 0);
    }

    void ApplyDeferredLighting() {
        if (g_currentRenderContext == CurrentRenderContext::Game) {
            if (!g_gameRenderTarget) return;

            UpdateTempRTReference();
            TransitionTarget(g_tempGameRT, D3D12_RESOURCE_STATE_RENDER_TARGET);

            // 1. G-Bufferリソースを RENDER_TARGET ➔ PIXEL_SHADER_RESOURCE にバリア遷移
            GBufferTarget* targets[6] = {
                &g_gameGBuffer.Color,
                &g_gameGBuffer.Normal,
                &g_gameGBuffer.Position,
                &g_gameGBuffer.Metallic,
                &g_gameGBuffer.Specular,
                &g_gameGBuffer.Roughness
            };
            for (int i = 0; i < 6; i++) {
                TransitionTarget(*targets[i], D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            }

            // 2. レンダーターゲットを本来の GameRenderTarget へバインド
            D3D12_CPU_DESCRIPTOR_HANDLE rtv = GetRtvHandle(3);
            D3D12_CPU_DESCRIPTOR_HANDLE dsv = GetDsvHandle(2);
            g_pd3dCommandList->OMSetRenderTargets(1, &rtv, FALSE, &dsv);

            // 3. Deferred PSO を取得してバインド
            ID3D12PipelineState* pso = ShaderManager::Instance().GetPipelineState("Deferred", "Deferred", 1, D3D12_CULL_MODE_NONE, false, false, D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
            if (pso) {
                g_pd3dCommandList->SetPipelineState(pso);
            }

            // 4. 定数バッファやG-Bufferテクスチャ(t0〜t5)のバインド
            BindShaderConstants();

            for (int i = 0; i < 6; i++) {
                D3D12_GPU_DESCRIPTOR_HANDLE handle = g_pd3dSrvDescHeap->GetGPUDescriptorHandleForHeapStart();
                handle.ptr += targets[i]->SrvIndex * g_srvDescriptorSize;
                g_pd3dCommandList->SetGraphicsRootDescriptorTable((UINT)(6 + i), handle);
            }

            // shadow
            D3D12_GPU_DESCRIPTOR_HANDLE shadowHandle = GetSrvGpuHandle(g_shadowMapSrvIndex);
            g_pd3dCommandList->SetGraphicsRootDescriptorTable(13, shadowHandle); // スロット13 = t7

            // 5. 全画面矩形を描画
            DrawFullScreenQuad();
        }
        else if (g_currentRenderContext == CurrentRenderContext::Scene) {
            if (!g_sceneRenderTarget) return;

            UpdateTempRTReference();
            TransitionTarget(g_tempSceneRT, D3D12_RESOURCE_STATE_RENDER_TARGET);

            // 1. G-Bufferリソースを RENDER_TARGET ➔ PIXEL_SHADER_RESOURCE にバリア遷移
            GBufferTarget* targets[6] = {
                &g_sceneGBuffer.Color,
                &g_sceneGBuffer.Normal,
                &g_sceneGBuffer.Position,
                &g_sceneGBuffer.Metallic,
                &g_sceneGBuffer.Specular,
                &g_sceneGBuffer.Roughness
            };
            for (int i = 0; i < 6; i++) {
                TransitionTarget(*targets[i], D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            }

            // 2. レンダーターゲットを本来の SceneRenderTarget へバインド
            D3D12_CPU_DESCRIPTOR_HANDLE rtv = GetRtvHandle(2);
            D3D12_CPU_DESCRIPTOR_HANDLE dsv = GetDsvHandle(1);
            g_pd3dCommandList->OMSetRenderTargets(1, &rtv, FALSE, &dsv);

            // 3. Deferred PSO を取得してバインド
            ID3D12PipelineState* pso = ShaderManager::Instance().GetPipelineState("Deferred", "Deferred", 1, D3D12_CULL_MODE_NONE, false, false, D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
            if (pso) {
                g_pd3dCommandList->SetPipelineState(pso);
            }

            // 4. 定数バッファやG-Bufferテクスチャ(t0〜t5)のバインド
            BindShaderConstants();

            for (int i = 0; i < 6; i++) {
                D3D12_GPU_DESCRIPTOR_HANDLE handle = g_pd3dSrvDescHeap->GetGPUDescriptorHandleForHeapStart();
                handle.ptr += targets[i]->SrvIndex * g_srvDescriptorSize;
                g_pd3dCommandList->SetGraphicsRootDescriptorTable((UINT)(6 + i), handle);
            }

            // shadow
            D3D12_GPU_DESCRIPTOR_HANDLE shadowHandle = GetSrvGpuHandle(g_shadowMapSrvIndex);
            g_pd3dCommandList->SetGraphicsRootDescriptorTable(13, shadowHandle); // スロット13 = t7

            // 5. 全画面矩形を描画
            DrawFullScreenQuad();
        }
    }

    // ======================================================================
    // ポストプロセスヘルパー: 全画面パスの実行
    // ======================================================================
    void RenderFullScreenPass(
        GBufferTarget& dst,
        GBufferTarget* srcTargets[], int srcCount,
        const char* vsId, const char* psId,
        float width, float height,
        const MATERIAL* paramOverride = nullptr)
    {
        // ソーステクスチャの状態遷移: PIXEL_SHADER_RESOURCE
        for (int i = 0; i < srcCount; i++) {
            TransitionTarget(*srcTargets[i], D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        }

        // デスティネーションをレンダーターゲットに遷移
        TransitionTarget(dst, D3D12_RESOURCE_STATE_RENDER_TARGET);

        // レンダーターゲット設定
        D3D12_CPU_DESCRIPTOR_HANDLE rtv = GetRtvHandle(dst.RtvIndex);
        g_pd3dCommandList->OMSetRenderTargets(1, &rtv, FALSE, nullptr);

        D3D12_VIEWPORT vp = { 0.0f, 0.0f, width, height, 0.0f, 1.0f };
        D3D12_RECT sr = { 0, 0, (LONG)width, (LONG)height };
        g_pd3dCommandList->RSSetViewports(1, &vp);
        g_pd3dCommandList->RSSetScissorRects(1, &sr);

        // PSO 取得
        ID3D12PipelineState* pso = ShaderManager::Instance().GetPipelineState(
            vsId, psId, 1, D3D12_CULL_MODE_NONE, false, false, D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
        if (pso) {
            g_pd3dCommandList->SetPipelineState(pso);
        }

        // パラメータ定数バッファ (Material スロット b3 を転用)
        if (paramOverride) {
            SetConstant(3, paramOverride, sizeof(MATERIAL));
        }

        // ソーステクスチャのバインド (t0, t1, ...)
        for (int i = 0; i < srcCount; i++) {
            D3D12_GPU_DESCRIPTOR_HANDLE handle = g_pd3dSrvDescHeap->GetGPUDescriptorHandleForHeapStart();
            handle.ptr += srcTargets[i]->SrvIndex * g_srvDescriptorSize;
            g_pd3dCommandList->SetGraphicsRootDescriptorTable((UINT)(6 + i), handle);
        }

        DrawFullScreenQuad();
    }

    // ======================================================================
    // Bloom 内部実装
    // ======================================================================
    void ApplyBloomInternal() {
        PostProcessBuffers& pp = (g_currentRenderContext == CurrentRenderContext::Game)
            ? g_gamePostProcess : g_scenePostProcess;

        GBufferTarget& sceneRT = (g_currentRenderContext == CurrentRenderContext::Game)
            ? g_tempGameRT : g_tempSceneRT;

        float baseWidth = (g_currentRenderContext == CurrentRenderContext::Game)
            ? g_gameWidth : g_sceneWidth;
        float baseHeight = (g_currentRenderContext == CurrentRenderContext::Game)
            ? g_gameHeight : g_sceneHeight;

        PostProcessSettings& settings = PostProcessSystem::GetSettings();

        // 1. 高輝度抽出 (Scene/Game RT → BrightTarget)
        {
            MATERIAL param = {};
            param.BaseColor = XMFLOAT4(settings.BloomThreshold, settings.BloomSoftKnee, settings.BloomIntensity, 0.0f);
            GBufferTarget* src[] = { &sceneRT };
            RenderFullScreenPass(pp.BrightTarget, src, 1, "Deferred", "BloomBright", baseWidth, baseHeight, &param);
        }

        // 2. ダウンサンプル (BrightTarget → BloomMips[0] → [1] → [2] → [3])
        {
            GBufferTarget* prevTarget = &pp.BrightTarget;
            float prevW = baseWidth;
            float prevH = baseHeight;

            for (int i = 0; i < 4; i++) {
                float mipW = baseWidth / (float)(2 << i);
                float mipH = baseHeight / (float)(2 << i);
                if (mipW < 1.0f) mipW = 1.0f;
                if (mipH < 1.0f) mipH = 1.0f;

                MATERIAL param = {};
                param.BaseColor = XMFLOAT4(1.0f / prevW, 1.0f / prevH, 0.0f, 0.0f);
                GBufferTarget* src[] = { prevTarget };
                RenderFullScreenPass(pp.BloomMips[i], src, 1, "Deferred", "BloomDownsample", mipW, mipH, &param);

                prevTarget = &pp.BloomMips[i];
                prevW = mipW;
                prevH = mipH;
            }
        }

        // 3. ガウシアンブラー (各段: BloomMips → BloomBlur → BloomMips)
        //    水平ブラー: BloomMips[i] → BloomBlur[i]
        //    垂直ブラー: BloomBlur[i] → BloomMips[i] (再利用)
        for (int i = 3; i >= 0; i--) {
            float mipW = baseWidth / (float)(2 << i);
            float mipH = baseHeight / (float)(2 << i);
            if (mipW < 1.0f) mipW = 1.0f;
            if (mipH < 1.0f) mipH = 1.0f;

            // 水平ブラー: BloomMips[i] → BloomBlur[i]
            {
                MATERIAL param = {};
                param.BaseColor = XMFLOAT4(1.0f / mipW, 1.0f / mipH, 1.0f, 0.0f); // isHorizontal = 1.0
                GBufferTarget* src[] = { &pp.BloomMips[i] };
                RenderFullScreenPass(pp.BloomBlur[i], src, 1, "Deferred", "BloomBlur", mipW, mipH, &param);
            }

            // 垂直ブラー: BloomBlur[i] → BloomMips[i]
            {
                MATERIAL param = {};
                param.BaseColor = XMFLOAT4(1.0f / mipW, 1.0f / mipH, 0.0f, 0.0f); // isHorizontal = 0.0
                GBufferTarget* src[] = { &pp.BloomBlur[i] };
                RenderFullScreenPass(pp.BloomMips[i], src, 1, "Deferred", "BloomBlur", mipW, mipH, &param);
            }

            // アップサンプル: 下位段のブラー結果を上位段に加算
            // BloomMips[i] の結果を上位段 (i-1) に加算合成
            if (i > 0) {
                float upperW = baseWidth / (float)(2 << (i - 1));
                float upperH = baseHeight / (float)(2 << (i - 1));
                if (upperW < 1.0f) upperW = 1.0f;
                if (upperH < 1.0f) upperH = 1.0f;

                MATERIAL param = {};
                param.BaseColor = XMFLOAT4(1.0f, 0.0f, 0.0f, 0.0f); // intensity = 1.0 for intermediate upsample
                GBufferTarget* src[] = { &pp.BloomMips[i - 1], &pp.BloomMips[i] };
                RenderFullScreenPass(pp.BloomBlur[i - 1], src, 2, "Deferred", "BloomComposite", upperW, upperH, &param);

                // BloomBlur[i-1] → BloomMips[i-1] にコピー（次のブラーパスの入力として）
                // BloomBlur[i-1] の結果を BloomMips[i-1] として使うために状態遷移
                {
                    TransitionTarget(pp.BloomBlur[i - 1], D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
                    TransitionTarget(pp.BloomMips[i - 1], D3D12_RESOURCE_STATE_RENDER_TARGET);

                    // BloomBlur[i-1] → BloomMips[i-1] のコピー（ダウンサンプルシェーダーでコピー描画）
                    MATERIAL copyParam = {};
                    copyParam.BaseColor = XMFLOAT4(1.0f / upperW, 1.0f / upperH, 0.0f, 0.0f);
                    
                    D3D12_CPU_DESCRIPTOR_HANDLE rtv = GetRtvHandle(pp.BloomMips[i - 1].RtvIndex);
                    g_pd3dCommandList->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
                    
                    D3D12_VIEWPORT vp = { 0.0f, 0.0f, upperW, upperH, 0.0f, 1.0f };
                    D3D12_RECT sr = { 0, 0, (LONG)upperW, (LONG)upperH };
                    g_pd3dCommandList->RSSetViewports(1, &vp);
                    g_pd3dCommandList->RSSetScissorRects(1, &sr);

                    ID3D12PipelineState* copyPso = ShaderManager::Instance().GetPipelineState(
                        "Deferred", "BloomDownsample", 1, D3D12_CULL_MODE_NONE, false, false, D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
                    if (copyPso) g_pd3dCommandList->SetPipelineState(copyPso);
                    
                    SetConstant(3, &copyParam, sizeof(copyParam));
                    
                    D3D12_GPU_DESCRIPTOR_HANDLE srvHandle = g_pd3dSrvDescHeap->GetGPUDescriptorHandleForHeapStart();
                    srvHandle.ptr += pp.BloomBlur[i - 1].SrvIndex * g_srvDescriptorSize;
                    g_pd3dCommandList->SetGraphicsRootDescriptorTable(6, srvHandle);
                    
                    DrawFullScreenQuad();
                }
            }
        }

        // 4. 最終合成 (Scene/Game RT + BloomMips[0] → Scene/Game RT)
        {
            // Scene/Game RT を SRV に遷移
            TransitionTarget(sceneRT, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

            // BloomMips[0] を SRV に遷移
            TransitionTarget(pp.BloomMips[0], D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

            // BrightTarget を一時的な合成先として使用
            TransitionTarget(pp.BrightTarget, D3D12_RESOURCE_STATE_RENDER_TARGET);

            D3D12_CPU_DESCRIPTOR_HANDLE rtv = GetRtvHandle(pp.BrightTarget.RtvIndex);
            g_pd3dCommandList->OMSetRenderTargets(1, &rtv, FALSE, nullptr);

            D3D12_VIEWPORT vp = { 0.0f, 0.0f, baseWidth, baseHeight, 0.0f, 1.0f };
            D3D12_RECT sr = { 0, 0, (LONG)baseWidth, (LONG)baseHeight };
            g_pd3dCommandList->RSSetViewports(1, &vp);
            g_pd3dCommandList->RSSetScissorRects(1, &sr);

            ID3D12PipelineState* pso = ShaderManager::Instance().GetPipelineState(
                "Deferred", "BloomComposite", 1, D3D12_CULL_MODE_NONE, false, false, D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
            if (pso) g_pd3dCommandList->SetPipelineState(pso);

            MATERIAL param = {};
            param.BaseColor = XMFLOAT4(settings.BloomIntensity, 0.0f, 0.0f, 0.0f);
            SetConstant(3, &param, sizeof(param));

            // t0: Scene/Game RT (元画像)
            D3D12_GPU_DESCRIPTOR_HANDLE sceneHandle = g_pd3dSrvDescHeap->GetGPUDescriptorHandleForHeapStart();
            sceneHandle.ptr += sceneRT.SrvIndex * g_srvDescriptorSize;
            g_pd3dCommandList->SetGraphicsRootDescriptorTable(6, sceneHandle);

            // t1: BloomMips[0] (ブラー済みBloom)
            D3D12_GPU_DESCRIPTOR_HANDLE bloomHandle = g_pd3dSrvDescHeap->GetGPUDescriptorHandleForHeapStart();
            bloomHandle.ptr += pp.BloomMips[0].SrvIndex * g_srvDescriptorSize;
            g_pd3dCommandList->SetGraphicsRootDescriptorTable(7, bloomHandle);

            DrawFullScreenQuad();

            // BrightTarget(合成結果)をSRVに → Scene/Game RTにコピー
            {
                TransitionTarget(pp.BrightTarget, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
                TransitionTarget(sceneRT, D3D12_RESOURCE_STATE_RENDER_TARGET);

                D3D12_CPU_DESCRIPTOR_HANDLE finalRtv = GetRtvHandle(sceneRT.RtvIndex);
                g_pd3dCommandList->OMSetRenderTargets(1, &finalRtv, FALSE, nullptr);
                g_pd3dCommandList->RSSetViewports(1, &vp);
                g_pd3dCommandList->RSSetScissorRects(1, &sr);

                // BrightTargetの内容をScene/Game RTにコピー描画
                ID3D12PipelineState* copyPso = ShaderManager::Instance().GetPipelineState(
                    "Deferred", "BloomDownsample", 1, D3D12_CULL_MODE_NONE, false, false, D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
                if (copyPso) g_pd3dCommandList->SetPipelineState(copyPso);

                MATERIAL copyParam = {};
                copyParam.BaseColor = XMFLOAT4(1.0f / baseWidth, 1.0f / baseHeight, 0.0f, 0.0f);
                SetConstant(3, &copyParam, sizeof(copyParam));

                D3D12_GPU_DESCRIPTOR_HANDLE brightHandle = g_pd3dSrvDescHeap->GetGPUDescriptorHandleForHeapStart();
                brightHandle.ptr += pp.BrightTarget.SrvIndex * g_srvDescriptorSize;
                g_pd3dCommandList->SetGraphicsRootDescriptorTable(6, brightHandle);

                DrawFullScreenQuad();
            }
        }
    }

    // ======================================================================
    // Color Grading 内部実装
    // ======================================================================
    void ApplyColorGradingInternal(){
        PostProcessBuffers& pp = (g_currentRenderContext == CurrentRenderContext::Game)
            ? g_gamePostProcess : g_scenePostProcess;
        GBufferTarget& sceneRT = (g_currentRenderContext == CurrentRenderContext::Game)
            ? g_tempGameRT : g_tempSceneRT;
        
    float baseWidth = (g_currentRenderContext == CurrentRenderContext::Game)
            ? g_gameWidth : g_sceneWidth;
    float baseHeight = (g_currentRenderContext == CurrentRenderContext::Game)
            ? g_gameHeight : g_sceneHeight;
    PostProcessSettings& settings = PostProcessSystem::GetSettings();
    // 1. カラーグレーディングパラメータの転送用マテリアル定義
    MATERIAL param = {};
    // BaseColor.x = Contrast, y = Saturation, z = Brightness
    param.BaseColor = XMFLOAT4(settings.Contrast, settings.Saturation, settings.Brightness, 0.0f);
    // EmissionColor = ColorFilter (RGB)
    param.EmissionColor = XMFLOAT4(settings.ColorFilter[0], settings.ColorFilter[1], settings.ColorFilter[2], 1.0f);
    // 2. sceneRT をソーステクスチャとしてカラーグレーディングを実行し、pp.BrightTarget へ描画
    GBufferTarget* src[] = { &sceneRT };
    RenderFullScreenPass(pp.BrightTarget, src, 1, "Deferred", "ColorGrading", baseWidth, baseHeight, &param);
    // 3. 処理結果 (pp.BrightTarget) を元の sceneRT へコピーして上書き
    TransitionTarget(pp.BrightTarget, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    TransitionTarget(sceneRT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    D3D12_CPU_DESCRIPTOR_HANDLE finalRtv = GetRtvHandle(sceneRT.RtvIndex);
    g_pd3dCommandList->OMSetRenderTargets(1, &finalRtv, FALSE, nullptr);
    
    D3D12_VIEWPORT vp = { 0.0f, 0.0f, baseWidth, baseHeight, 0.0f, 1.0f };
    D3D12_RECT sr = { 0, 0, (LONG)baseWidth, (LONG)baseHeight };
    g_pd3dCommandList->RSSetViewports(1, &vp);
    g_pd3dCommandList->RSSetScissorRects(1, &sr);
    ID3D12PipelineState* copyPso = ShaderManager::Instance().GetPipelineState(
        "Deferred", "BloomDownsample", 1, D3D12_CULL_MODE_NONE, false, false, D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
    if (copyPso) g_pd3dCommandList->SetPipelineState(copyPso);
    MATERIAL copyParam = {};
    copyParam.BaseColor = XMFLOAT4(1.0f / baseWidth, 1.0f / baseHeight, 0.0f, 0.0f);
    SetConstant(3, &copyParam, sizeof(copyParam));
    D3D12_GPU_DESCRIPTOR_HANDLE brightHandle = g_pd3dSrvDescHeap->GetGPUDescriptorHandleForHeapStart();
    brightHandle.ptr += pp.BrightTarget.SrvIndex * g_srvDescriptorSize;
    g_pd3dCommandList->SetGraphicsRootDescriptorTable(6, brightHandle);
    DrawFullScreenQuad();
        
        
    }

    // ======================================================================
    // Depth of Field 内部実装
    // ======================================================================
    void ApplyDepthOfFieldInternal() {
    PostProcessBuffers& pp = (g_currentRenderContext == CurrentRenderContext::Game)
        ? g_gamePostProcess : g_scenePostProcess;
    GBufferTarget& sceneRT = (g_currentRenderContext == CurrentRenderContext::Game)
        ? g_tempGameRT : g_tempSceneRT;
    GBufferSet& gbuffer = (g_currentRenderContext == CurrentRenderContext::Game)
        ? g_gameGBuffer : g_sceneGBuffer;
    float baseWidth = (g_currentRenderContext == CurrentRenderContext::Game) ? g_gameWidth : g_sceneWidth;
    float baseHeight = (g_currentRenderContext == CurrentRenderContext::Game) ? g_gameHeight : g_sceneHeight;
    PostProcessSettings& settings = PostProcessSystem::GetSettings();
    // --- 1. 元画像を 1/2 サイズバッファに縮小しながら水平ブラー ---
    MATERIAL blurHParam = {};
    blurHParam.BaseColor = XMFLOAT4(1.0f / baseWidth, 1.0f / baseHeight, 1.0f, 0.0f); // Z=1.0: 水平
    GBufferTarget* srcH[] = { &sceneRT };
    RenderFullScreenPass(pp.BloomMips[0], srcH, 1, "Deferred", "BloomBlur", baseWidth / 2.0f, baseHeight / 2.0f, &blurHParam);
    // --- 2. 1/2 サイズバッファを垂直ブラー (pp.BloomBlur[0] に格納) ---
    MATERIAL blurVParam = {};
    blurVParam.BaseColor = XMFLOAT4(2.0f / baseWidth, 2.0f / baseHeight, 0.0f, 0.0f); // Z=0.0: 垂直
    GBufferTarget* srcV[] = { &pp.BloomMips[0] };
    RenderFullScreenPass(pp.BloomBlur[0], srcV, 1, "Deferred", "BloomBlur", baseWidth / 2.0f, baseHeight / 2.0f, &blurVParam);
    // --- 3. DoF 合成 (元画像 + ブラー画像 + ワールド座標) を実行し pp.BrightTarget (等倍) へ描画 ---
    MATERIAL dofParam = {};
    dofParam.BaseColor = XMFLOAT4(settings.DofFocusDistance, settings.DofFocusRange, settings.DofBlurIntensity, 0.0f);
    // ソース: [0]元画像 (t0), [1]ブラー画像 (t1), [2]座標バッファ (t2)
    GBufferTarget* dofSrc[] = { &sceneRT, &pp.BloomBlur[0], &gbuffer.Position };
    RenderFullScreenPass(pp.BrightTarget, dofSrc, 3, "Deferred", "DepthOfField", baseWidth, baseHeight, &dofParam);
    // --- 4. 合成完了した pp.BrightTarget の内容を最終出力である sceneRT に書き戻す ---
    TransitionTarget(pp.BrightTarget, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    TransitionTarget(sceneRT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    D3D12_CPU_DESCRIPTOR_HANDLE finalRtv = GetRtvHandle(sceneRT.RtvIndex);
    g_pd3dCommandList->OMSetRenderTargets(1, &finalRtv, FALSE, nullptr);
    
    D3D12_VIEWPORT vp = { 0.0f, 0.0f, baseWidth, baseHeight, 0.0f, 1.0f };
    D3D12_RECT sr = { 0, 0, (LONG)baseWidth, (LONG)baseHeight };
    g_pd3dCommandList->RSSetViewports(1, &vp);
    g_pd3dCommandList->RSSetScissorRects(1, &sr);
    ID3D12PipelineState* copyPso = ShaderManager::Instance().GetPipelineState(
        "Deferred", "BloomDownsample", 1, D3D12_CULL_MODE_NONE, false, false, D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
    if (copyPso) g_pd3dCommandList->SetPipelineState(copyPso);
    MATERIAL copyParam = {};
    copyParam.BaseColor = XMFLOAT4(1.0f / baseWidth, 1.0f / baseHeight, 0.0f, 0.0f);
    SetConstant(3, &copyParam, sizeof(copyParam));
    D3D12_GPU_DESCRIPTOR_HANDLE brightHandle = g_pd3dSrvDescHeap->GetGPUDescriptorHandleForHeapStart();
    brightHandle.ptr += pp.BrightTarget.SrvIndex * g_srvDescriptorSize;
    g_pd3dCommandList->SetGraphicsRootDescriptorTable(6, brightHandle);
    DrawFullScreenQuad();
}

    // ======================================================================
    // ApplyPostProcess: 汎用ポストプロセスエントリーポイント
    // ======================================================================
    void ApplyPostProcess() {
        if (g_currentRenderContext == CurrentRenderContext::None) return;

        bool isGame = (g_currentRenderContext == CurrentRenderContext::Game);
        ComPtr<ID3D12Resource>& rt = isGame ? g_gameRenderTarget : g_sceneRenderTarget;
        if (!rt) return;

        UpdateTempRTReference();

        PostProcessSettings& settings = PostProcessSystem::GetSettings();

        // Bloom
        if (settings.BloomEnabled) {
            ApplyBloomInternal();
        }

        // Color Grading
        if(settings.ColorGradingEnabled){
            ApplyColorGradingInternal();
        }

        // Depth of Field
        if(settings.DofEnabled){
            ApplyDepthOfFieldInternal();
        }

    }

    void* GetGBufferSRV(int bufferIndex, bool isGame) {
        const GBufferSet& gbuffer = isGame ? g_gameGBuffer : g_sceneGBuffer;
        unsigned int srvIndex = 0;
        switch (bufferIndex) {
            case 0: srvIndex = gbuffer.Color.SrvIndex; break;
            case 1: srvIndex = gbuffer.Normal.SrvIndex; break;
            case 2: srvIndex = gbuffer.Position.SrvIndex; break;
            case 3: srvIndex = gbuffer.Metallic.SrvIndex; break;
            case 4: srvIndex = gbuffer.Specular.SrvIndex; break;
            case 5: srvIndex = gbuffer.Roughness.SrvIndex; break;
            default: return nullptr;
        }
        D3D12_GPU_DESCRIPTOR_HANDLE handle = GetSrvGpuHandle(srvIndex);
        return (void*)handle.ptr;
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
