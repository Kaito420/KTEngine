#include <windows.h>
#include <time.h>
#include "ImGuiLayer.h"
#include "imgui.h"
#include "imgui/ImGuizmo.h"
#include <DirectXMath.h>
#include "Renderer.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx11.h"

#include "Manager.h"
#include "Camera.h"
#include "FileBrowser.h"
#include "Scene.h"
#include "Input.h"

// グローバル変数
HWND hwnd = nullptr;
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);                // Use ImGui::GetCurrentContext()
FileBrowser fileBrowser;

// ウィンドウプロシージャ
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg) {
    case WM_INPUT:
        Input::ProcessRawInput(lParam);
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {

    srand((unsigned)time(NULL));
    // ウィンドウクラス登録
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0L, 0L,
                      hInstance, NULL, NULL, NULL, NULL,
                      "DX11WindowClass", NULL };
    RegisterClassEx(&wc);
    hwnd = CreateWindow(wc.lpszClassName, "Engine",
        WS_OVERLAPPEDWINDOW, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT,
        NULL, NULL, wc.hInstance, NULL);

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    Manager::LoadEngineConfig();
    // DirectX12 初期化
    if (!Renderer::Init(hwnd)) return 1;

    // ImGui 初期化
    ImGuiLayer::Init(hwnd);

    // レンダーターゲット初期化
    Renderer::InitSceneRenderTarget(1280, 720);
    Renderer::InitGameRenderTarget(1280, 720);

    // メインループ
    MSG msg = {};
    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            continue;
        }

        // ゲーム描画
        Renderer::BeginGameRender();
        // Manager::Render(); // コメントアウト

        // シーン描画
        Renderer::BeginSceneRender();
        // Manager::Render(); // コメントアウト

        // バックバッファ描画とImGui
        Renderer::BeginFrame();
        ImGuiLayer::Begin();

        // テストウィンドウ表示
        ImGui::ShowDemoWindow();
        ImGui::Begin("KTEngine Pure D3D12 Test");
        ImGui::Text("DirectX 12 Pure Migration Phase 1 Success!");
        ImGui::End();

        ImGuiLayer::End();
        Renderer::EndFrame();

        Input::Update();    // Inputの更新
    }

    // クリーンアップ
    ImGuiLayer::Shutdown();
    Renderer::Shutdown();
    UnregisterClass(wc.lpszClassName, hInstance);
    return 0;
}

