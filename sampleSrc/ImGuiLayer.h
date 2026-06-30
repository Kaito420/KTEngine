// ======================================================================
// ImGuiLayer.h
// aurhor: Kaito Aoki
// Date: 2026/04/13
// ======================================================================
#ifndef _IMGUI_LAYER_H_
#define _IMGUI_LAYER_H_

#include <windows.h>
#include <d3d12.h>
#include <wrl/client.h>

namespace ImGuiLayer {

    // 初期化にデバイスやフォーマット情報が必要
    void Init(HWND hwnd, ID3D12Device* device, ID3D12CommandQueue* commandQueue, int numFramesInFlight, DXGI_FORMAT rtvFormat,
        ID3D12DescriptorHeap* srvHeap, D3D12_CPU_DESCRIPTOR_HANDLE fontCpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE fontGpuHandle);
    // UI構築の開始
    void NewFrame();
    // 構築したUIをコマンドリストに積んで描画
    void RenderDrawData(ID3D12GraphicsCommandList* commandList);

    void Shutdown();
}

#endif // !_IMGUI_LAYER_H_