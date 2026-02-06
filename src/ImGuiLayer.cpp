//=====================================================================================
// ImGuiLayer.cpp
// Author:Kaito Aoki
// Date:2025/06/23
//=====================================================================================

#include "ImGuiLayer.h"

#include "ImGuiLayer.h"
#include "imgui.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx11.h"

#include "RendererDX11.h"

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
    ImGui_ImplDX11_Init(device, context);
}

void ImGuiLayer::Begin() {
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

	ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_PassthruCentralNode/* | ImGuiDockNodeFlags_NoDockingInCentralNode*/;

	ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(),dockspace_flags);
}

void ImGuiLayer::End() {
    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    ImGuiIO& io = ImGui::GetIO();
    if(io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        
        ID3D11DeviceContext* context = RendererDX11::GetContext();
        
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault(NULL, (void*)context);

	}
}

void ImGuiLayer::Shutdown() {
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}