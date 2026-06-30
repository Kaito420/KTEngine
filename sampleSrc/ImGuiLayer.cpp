// ======================================================================
// ImGuiLayer.cpp
// aurhor: Kaito Aoki
// Date: 2026/04/13
// ======================================================================

#include "ImGuiLayer.h"
#include <cassert>
#include "imgui.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx12.h"


namespace ImGuiLayer {

	void Init(HWND hwnd, ID3D12Device* device, ID3D12CommandQueue* commandQueue, int numFramesInFlight, DXGI_FORMAT rtvFormat,
		ID3D12DescriptorHeap* srvHeap, D3D12_CPU_DESCRIPTOR_HANDLE fontCpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE fontGpuHandle){
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO(); (void)io;
		ImGui::StyleColorsDark();

		// 専用ヒープを作る処理（CreateDescriptorHeapなど）はすべて削除

		ImGui_ImplWin32_Init(hwnd);

		ImGui_ImplDX12_InitInfo initInfo = {};
		initInfo.Device = device;
		initInfo.CommandQueue = commandQueue;
		initInfo.NumFramesInFlight = numFramesInFlight;
		initInfo.RTVFormat = rtvFormat;
		// 引数で受け取った汎用ヒープとハンドルを渡す
		initInfo.SrvDescriptorHeap = srvHeap;
		initInfo.LegacySingleSrvCpuDescriptor = fontCpuHandle;
		initInfo.LegacySingleSrvGpuDescriptor = fontGpuHandle;
		ImGui_ImplDX12_Init(&initInfo);
	}

	void NewFrame() {
		// 新しいフレームの開始
		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();
	}
	void RenderDrawData(ID3D12GraphicsCommandList* commandList) {
		// UIの構築を終了
		ImGui::Render();

		// 描画コマンドを積む
		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);
	}
	void Shutdown() {
		// バックエンドのシャットダウンとコンテキストの破棄
		ImGui_ImplDX12_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();

	}
}
