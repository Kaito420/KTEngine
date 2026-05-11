import codecs

dx12_cpp = '''#include "Renderer.h"
#include <iostream>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
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

        DXGI_SWAP_CHAIN_DESC1 sd = {};
        sd.BufferCount = FrameCount;
        sd.Width = 1920;
        sd.Height = 1080;
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

        // Init ImGui DX12
        ImGui_ImplDX12_Init(g_pd3dDevice.Get(), FrameCount,
            DXGI_FORMAT_R8G8B8A8_UNORM, g_pd3dSrvDescHeap.Get(),
            g_pd3dSrvDescHeap->GetCPUDescriptorHandleForHeapStart(),
            g_pd3dSrvDescHeap->GetGPUDescriptorHandleForHeapStart());

        return true;
    }

    void Shutdown() {
        WaitForLastSubmittedFrame();
        ImGui_ImplDX12_Shutdown();
        CloseHandle(g_fenceEvent);
    }

    void BeginFrame() {
        ImGui_ImplDX12_NewFrame();
        
        g_commandAllocators[g_frameIndex]->Reset();
        g_pd3dCommandList->Reset(g_commandAllocators[g_frameIndex].Get(), nullptr);

        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource = g_mainRenderTargetResource[g_frameIndex].Get();
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        g_pd3dCommandList->ResourceBarrier(1, &barrier);

        SIZE_T rtvDescriptorSize = g_pd3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = g_pd3dRtvDescHeap->GetCPUDescriptorHandleForHeapStart();
        rtvHandle.ptr += g_frameIndex * rtvDescriptorSize;
        
        g_pd3dCommandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

        const float clearColor[] = { 0.1f, 0.2f, 0.3f, 1.0f }; // Nice dark blue for DX12 mode
        g_pd3dCommandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
    }

    void EndFrame() {
        ID3D12DescriptorHeap* descriptorHeaps[] = { g_pd3dSrvDescHeap.Get() };
        g_pd3dCommandList->SetDescriptorHeaps(1, descriptorHeaps);
        ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), g_pd3dCommandList.Get());

        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource = g_mainRenderTargetResource[g_frameIndex].Get();
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
        g_pd3dCommandList->ResourceBarrier(1, &barrier);

        g_pd3dCommandList->Close();

        ID3D12CommandList* ppCommandLists[] = { g_pd3dCommandList.Get() };
        g_pd3dCommandQueue->ExecuteCommandLists(1, ppCommandLists);

        g_pSwapChain->Present(1, 0);

        WaitForLastSubmittedFrame();
    }

    bool InitSceneRenderTarget(int width, int height) { return true; }
    void BeginSceneRender() {}
    void* GetSceneSRV() { return nullptr; }
    void ResizeSceneBuffer(float width, float height) {}
}
'''
with codecs.open(r'c:\Users\kaito\source\repos\KTEngine\src\RendererDX12.cpp', 'w', 'cp932') as f:
    f.write(dx12_cpp)

imgui_layer_cpp = '''#include "ImGuiLayer.h"
#include "imgui.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx11.h"
#include "backends/imgui_impl_dx12.h"
#include "Renderer.h"

void ImGuiLayer::Init(HWND hwnd, ID3D11Device* device, ID3D11DeviceContext* context) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;

    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();

    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    ImGui_ImplWin32_Init(hwnd);
    if (Renderer::GetGraphicsAPI() == GraphicsAPI::DirectX11) {
        ImGui_ImplDX11_Init(device, context);
    }
    // DX12 Init is handled inside RendererDX12::Init for simplicity as it requires specific descriptors
}

void ImGuiLayer::Begin() {
    if (Renderer::GetGraphicsAPI() == GraphicsAPI::DirectX11) {
        ImGui_ImplDX11_NewFrame();
    }
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_PassthruCentralNode;
    ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), dockspace_flags);
}

void ImGuiLayer::End() {
    ImGui::Render();
    if (Renderer::GetGraphicsAPI() == GraphicsAPI::DirectX11) {
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    }
    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
    }
}

void ImGuiLayer::Shutdown() {
    if (Renderer::GetGraphicsAPI() == GraphicsAPI::DirectX11) {
        ImGui_ImplDX11_Shutdown();
    }
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}
'''
with codecs.open(r'c:\Users\kaito\source\repos\KTEngine\src\ImGuiLayer.cpp', 'w', 'cp932') as f:
    f.write(imgui_layer_cpp)

print('Generated RendererDX12 and ImGuiLayer')
