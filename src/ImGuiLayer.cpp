#include "ImGuiLayer.h"
#include "imgui.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx12.h"
#include "Renderer.h"
#include <d3d12.h>

void ImGuiLayer::Init(HWND hwnd) {
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

    // D3D12 initialization info
    ID3D12Device* device = Renderer::GetDeviceDX12();
    ID3D12DescriptorHeap* srvHeap = Renderer::GetSrvHeapDX12();
    ID3D12CommandQueue* queue = Renderer::GetCommandQueueDX12();

    D3D12_CPU_DESCRIPTOR_HANDLE fontCpuHandle = srvHeap->GetCPUDescriptorHandleForHeapStart();
    D3D12_GPU_DESCRIPTOR_HANDLE fontGpuHandle = srvHeap->GetGPUDescriptorHandleForHeapStart();

    ImGui_ImplDX12_InitInfo initInfo = {};
    initInfo.Device = device;
    initInfo.CommandQueue = queue;
    initInfo.NumFramesInFlight = 2;
    initInfo.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    initInfo.SrvDescriptorHeap = srvHeap;
    initInfo.LegacySingleSrvCpuDescriptor = fontCpuHandle;
    initInfo.LegacySingleSrvGpuDescriptor = fontGpuHandle;

    ImGui_ImplDX12_Init(&initInfo);
}

void ImGuiLayer::Begin() {
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_PassthruCentralNode;
    ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), dockspace_flags);
}

void ImGuiLayer::End() {
    ImGui::Render();
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), Renderer::GetCommandListDX12());
    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
    }
}

void ImGuiLayer::Shutdown() {
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}
