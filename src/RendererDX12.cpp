#include "Renderer.h"
#include <iostream>
#include <d3d12.h>
#include <d3d11on12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxgi.lib")

#include "imgui.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx12.h"

using Microsoft::WRL::ComPtr;

namespace RendererDX12 {
    const int FrameCount = 2;
    ComPtr<ID3D12Device> g_pd3dDevice = nullptr;
    ComPtr<ID3D12DescriptorHeap> g_pd3dRtvDescHeap = nullptr;
    ComPtr<ID3D12DescriptorHeap> g_pd3dSrvDescHeap = nullptr;
    ComPtr<ID3D12CommandQueue> g_pd3dCommandQueue = nullptr;
    ComPtr<ID3D12GraphicsCommandList> g_pd3dCommandList = nullptr;
    ComPtr<ID3D12CommandAllocator> g_commandAllocators[FrameCount];
    ComPtr<IDXGISwapChain3> g_pSwapChain = nullptr;
    ComPtr<ID3D12Resource> g_mainRenderTargetResource[FrameCount];
    ComPtr<ID3D12Fence> g_fence = nullptr;
    HANDLE g_fenceEvent = nullptr;
    UINT64 g_fenceLastSignaledValue = 0;
    UINT g_frameIndex = 0;

    ComPtr<ID3D11Device> g_pd3d11Device = nullptr;
    ComPtr<ID3D11DeviceContext> g_pd3d11Context = nullptr;
    ComPtr<ID3D11On12Device> g_pd3d11On12Device = nullptr;
    ComPtr<ID3D11Resource> g_wrappedBackBuffers[FrameCount];
    ComPtr<ID3D11RenderTargetView> g_d3d11RenderTargetViews[FrameCount];

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

        D3D12_DESCRIPTOR_HEAP_DESC rtvDesc = {};
        rtvDesc.NumDescriptors = FrameCount;
        rtvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rtvDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        g_pd3dDevice->CreateDescriptorHeap(&rtvDesc, IID_PPV_ARGS(&g_pd3dRtvDescHeap));

        SIZE_T rtvDescriptorSize = g_pd3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = g_pd3dRtvDescHeap->GetCPUDescriptorHandleForHeapStart();
        for (UINT i = 0; i < FrameCount; i++) {
            g_pSwapChain->GetBuffer(i, IID_PPV_ARGS(&g_mainRenderTargetResource[i]));
            g_pd3dDevice->CreateRenderTargetView(g_mainRenderTargetResource[i].Get(), nullptr, rtvHandle);
            rtvHandle.ptr += rtvDescriptorSize;
            g_pd3dDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&g_commandAllocators[i]));
        }

        D3D12_DESCRIPTOR_HEAP_DESC srvDesc = {};
        srvDesc.NumDescriptors = 1;
        srvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        srvDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        g_pd3dDevice->CreateDescriptorHeap(&srvDesc, IID_PPV_ARGS(&g_pd3dSrvDescHeap));

        g_pd3dDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, g_commandAllocators[0].Get(), nullptr, IID_PPV_ARGS(&g_pd3dCommandList));
        g_pd3dCommandList->Close();

        g_pd3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&g_fence));
        g_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);

        // Initialize D3D11On12
        UINT d3d11DeviceFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#if defined(_DEBUG)
        // d3d11DeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
        ComPtr<ID3D11Device> d3d11Device;
        if (FAILED(D3D11On12CreateDevice(
            g_pd3dDevice.Get(),
            d3d11DeviceFlags,
            nullptr,
            0,
            reinterpret_cast<IUnknown**>(g_pd3dCommandQueue.GetAddressOf()),
            1,
            0,
            &d3d11Device,
            &g_pd3d11Context,
            nullptr
        ))) return false;

        if (FAILED(d3d11Device.As(&g_pd3d11On12Device))) return false;
        if (FAILED(d3d11Device.As(&g_pd3d11Device))) return false;

        // Create wrapped resources for back buffers
        for (UINT i = 0; i < FrameCount; i++) {
            D3D11_RESOURCE_FLAGS d3d11Flags = { D3D11_BIND_RENDER_TARGET };
            if (FAILED(g_pd3d11On12Device->CreateWrappedResource(
                g_mainRenderTargetResource[i].Get(),
                &d3d11Flags,
                D3D12_RESOURCE_STATE_RENDER_TARGET,
                D3D12_RESOURCE_STATE_PRESENT,
                IID_PPV_ARGS(&g_wrappedBackBuffers[i])
            ))) return false;

            if (FAILED(g_pd3d11Device->CreateRenderTargetView(g_wrappedBackBuffers[i].Get(), nullptr, &g_d3d11RenderTargetViews[i]))) return false;
        }

        // Initialize DX11 states using D3D11On12
        RendererDX11::SetExternalDevice(g_pd3d11Device.Get(), g_pd3d11Context.Get());
        
        if (!RendererDX11::InitState(width, height)) return false;

        return true;
    }

    void Shutdown() {
        WaitForLastSubmittedFrame();
        
        for (UINT i = 0; i < FrameCount; i++) {
            g_d3d11RenderTargetViews[i].Reset();
            g_wrappedBackBuffers[i].Reset();
        }
        g_pd3d11Context->ClearState();
        g_pd3d11Context->Flush();
        g_pd3d11Context.Reset();
        g_pd3d11On12Device.Reset();
        g_pd3d11Device.Reset();

        CloseHandle(g_fenceEvent);
    }

    
    ID3D12Device* GetDevice() { return g_pd3dDevice.Get(); }
    ID3D12DescriptorHeap* GetSrvHeap() { return g_pd3dSrvDescHeap.Get(); }
    int GetFrameCount() { return FrameCount; }
    ID3D12GraphicsCommandList* GetCommandList() { return g_pd3dCommandList.Get(); }
    ID3D12CommandQueue* GetCommandQueue() { return g_pd3dCommandQueue.Get(); }

    ID3D11Device* GetDevice11() { return g_pd3d11Device.Get(); }
    ID3D11DeviceContext* GetContext11() { return g_pd3d11Context.Get(); }

    void BeginFrame() {
        
        
        g_commandAllocators[g_frameIndex]->Reset();
        g_pd3dCommandList->Reset(g_commandAllocators[g_frameIndex].Get(), nullptr);

        // Acquire wrapped resources
        g_pd3d11On12Device->AcquireWrappedResources(g_wrappedBackBuffers[g_frameIndex].GetAddressOf(), 1);
        
        // Clear via D3D11
        const float clearColor11[] = { 0.1f, 0.2f, 0.3f, 1.0f }; // Nice dark blue for DX12 mode
        ID3D11DepthStencilView* dsv = RendererDX11::GetDSV();
        // Bind ONLY the render target. ImGui doesn't need depth, and binding a native DSV with a wrapped RTV can fail!
        g_pd3d11Context->OMSetRenderTargets(1, g_d3d11RenderTargetViews[g_frameIndex].GetAddressOf(), nullptr);
        g_pd3d11Context->ClearRenderTargetView(g_d3d11RenderTargetViews[g_frameIndex].Get(), clearColor11);
        if (dsv) {
            g_pd3d11Context->ClearDepthStencilView(dsv, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
        }
    }

    void EndFrame() {
        // Unbind the render target before releasing the wrapped resource!
        ID3D11RenderTargetView* nullViews[] = { nullptr };
        g_pd3d11Context->OMSetRenderTargets(1, nullViews, nullptr);

        // Release wrapped resources
        g_pd3d11On12Device->ReleaseWrappedResources(g_wrappedBackBuffers[g_frameIndex].GetAddressOf(), 1);
        g_pd3d11Context->Flush();

        g_pSwapChain->Present(1, 0);

        WaitForLastSubmittedFrame();
    }

    bool InitSceneRenderTarget(int width, int height) { return true; }
    void BeginSceneRender() {}
    void* GetSceneSRV() { return nullptr; }
    void ResizeSceneBuffer(float width, float height) {}
}


bool RendererDX12::InitGameRenderTarget(int width, int height) { return true; }
void RendererDX12::BeginGameRender() {}
void* RendererDX12::GetGameSRV() { return nullptr; }
void RendererDX12::ResizeGameBuffer(float width, float height) {}
