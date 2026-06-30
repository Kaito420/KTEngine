#include "Renderer.h"
#include "D3DX12.h"
#include <iostream>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
#include <cassert>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

#include "imgui.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx12.h"

using Microsoft::WRL::ComPtr;

namespace RendererDX12 {
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

    // ビューポート/シザー
    D3D12_VIEWPORT g_viewport = {};
    D3D12_RECT g_scissorRect = {};

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
            WaitForSingleObject(g_fenceEvent, INFINITE);
        }
        g_frameIndex = g_pSwapChain->GetCurrentBackBufferIndex();
    }

    bool Init(HWND hwnd) {
        ComPtr<IDXGIFactory4> factory;
        if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))) return false;

        if (FAILED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&g_pd3dDevice)))) return false;

        D3D12_COMMAND_QUEUE_DESC queueDesc = {};
        queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        queueDesc.Flags = g_pd3dDevice->GetDeviceRemovedReason() == S_OK ? D3D12_COMMAND_QUEUE_FLAG_NONE : D3D12_COMMAND_QUEUE_FLAG_NONE; // Dummy logic to keep simple
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
        srvHeapDesc.NumDescriptors = 64;
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
            g_mainRenderTargetResource[i].Reset();
            g_commandAllocators[i].Reset();
        }

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

    ID3D11Device* GetDevice11() { return nullptr; }
    ID3D11DeviceContext* GetContext11() { return nullptr; }

    void BeginGameRender() {
        g_commandAllocators[g_frameIndex]->Reset();
        g_pd3dCommandList->Reset(g_commandAllocators[g_frameIndex].Get(), nullptr);

        ID3D12DescriptorHeap* descriptorHeaps[] = { g_pd3dSrvDescHeap.Get() };
        g_pd3dCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

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
}
