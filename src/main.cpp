#include <windows.h>
#include "ImGuiLayer.h"
#include "RendererDX11.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx11.h"
#include "imgui.h"

#include "Manager.h"

#include "Square.h"

// グローバル変数
HWND hwnd = nullptr;
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);                // Use ImGui::GetCurrentContext()

// ウィンドウプロシージャ
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg) {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    // ウィンドウクラス登録
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0L, 0L,
                      hInstance, NULL, NULL, NULL, NULL,
                      "DX11WindowClass", NULL };
    RegisterClassEx(&wc);
    hwnd = CreateWindow(wc.lpszClassName, "Engine",
        WS_OVERLAPPEDWINDOW, 100, 100, 1280, 800,
        NULL, NULL, wc.hInstance, NULL);

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    // DirectX11 初期化
    if (!RendererDX11::Init(hwnd)) return 1;

    // ImGui 初期化
    ImGuiLayer::Init(hwnd, RendererDX11::GetDevice(), RendererDX11::GetContext());

    //Manager 初期化
    Manager::Initialize();


    //テスト用
    //============================================
        Square test;
        test.Awake();

        int num = 0;
    //============================================


    // メインループ
    MSG msg = {};
    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            continue;
        }

        Manager::Update(); // Managerの更新

        //テスト用Update
        {

        }

        RendererDX11::BeginFrame();

		Manager::Render(); // Managerの描画

        //テスト用Render
        {
            test.Render();
        }

        ImGuiLayer::Begin();
        {
            
            ImGui::Begin("Hello ImGui");
            ImGui::Text("This is ImGui inside DirectX11!");
            ImGui::Button("Button", { 100,50 });
            ImGui::SliderInt("sliderInt", &num, -100, 100);
            ImGui::End();

            ImGui::Begin("Another Window");
            ImGui::Text("This is another window!");
            ImGui::Button("Another Button", { 150, 50 });
			ImGui::End();

        }
        ImGuiLayer::End();

        

        RendererDX11::EndFrame();
    }

    // クリーンアップ
    ImGuiLayer::Shutdown();
    RendererDX11::Shutdown();
    UnregisterClass(wc.lpszClassName, hInstance);
    return 0;
}

