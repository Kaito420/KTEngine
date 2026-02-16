#include <windows.h>
#include <time.h>
#include "ImGuiLayer.h"
#include "RendererDX11.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx11.h"
#include "imgui.h"

#include "Manager.h"
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

    // DirectX11 初期化
    if (!RendererDX11::Init(hwnd)) return 1;

    // ImGui 初期化
    ImGuiLayer::Init(hwnd, RendererDX11::GetDevice(), RendererDX11::GetContext());

    //Manager 初期化
    Manager::Initialize();
    Input::Initialize(hwnd);

    //シーン用バッファ（一旦サイズ固定）
    RendererDX11::InitSceneRenderTarget(1280,720);

    // メインループ
    MSG msg = {};
    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            continue;
        }

        Manager::Update(); // Managerの更新

        //ゲームシーンをテクスチャにレンダリング
        RendererDX11::BeginSceneRender();
        Manager::Render();

        //ImGuiとウィンドウ全体のレンダリング
        RendererDX11::BeginFrame();

		//Manager::Render(); // Managerの描画


        ImGuiLayer::Begin();
        {
			Manager::RenderMenuBar();
            if (Manager::IsShowContentBrowser())
                fileBrowser.Render();
            if (Manager::IsShowHierarchy())
                Manager::GetCurrentScene()->RenderHierarchy();
            if (Manager::IsShowInspector())
                Manager::GetCurrentScene()->RenderInspector();
            Manager::GetCurrentScene()->RenderButton();

            if (Manager::IsShowGameView()) {
                ImGui::Begin("Game View", nullptr, ImGuiWindowFlags_NoScrollbar);//ゲームビュー描画
                {
                    ImVec2 viewportSize = ImGui::GetContentRegionAvail(); //描画領域のサイズ取得

                    //サイズが有効かつ現在のテクスチャサイズと異なる場合はリサイズ
					RendererDX11::ResizeSceneBuffer(viewportSize.x, viewportSize.y);

                    void* myTexture = RendererDX11::GetSceneSRV();
                    if (myTexture == nullptr)
                        ImGui::Text("Texture is NULL!");
                    else {
                        ImGui::Image(myTexture, viewportSize);
                        bool isHovered = ImGui::IsItemHovered();
                        Input::SetGameViewHovered(isHovered);
                    }
                }
            ImGui::End();
            }
        }
        ImGuiLayer::End();


        RendererDX11::EndFrame();

        Input::Update();    //Inputの更新

    }

    // クリーンアップ
	Manager::Finalize();
    ImGuiLayer::Shutdown();
    RendererDX11::Shutdown();
    UnregisterClass(wc.lpszClassName, hInstance);
    return 0;
}

