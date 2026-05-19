#include <windows.h>
#include <time.h>
#include "ImGuiLayer.h"
#include "Renderer.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx11.h"
#include "imgui.h"

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
    // DirectX11 初期化
    if (!Renderer::Init(hwnd)) return 1;

    // ImGui 初期化
    ImGuiLayer::Init(hwnd, Renderer::GetDevice(), Renderer::GetContext());

    //Manager 初期化
    Manager::Initialize();
    Input::Initialize(hwnd);

    //シーン用バッファ（一旦サイズ固定）
    Renderer::InitSceneRenderTarget(1280,720);
    Renderer::InitGameRenderTarget(1280,720);

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
        Renderer::BeginGameRender();
        Camera* mainCamera = nullptr;
        if (Manager::GetCurrentScene()) {
            mainCamera = Manager::GetCurrentScene()->FindGameObjectByName<Camera>("Camera");
        }
        if (mainCamera) {
            Renderer::SetViewMatrix(mainCamera->GetViewMatrix());
            Renderer::SetProjectionMatrix(mainCamera->GetProjectionMatrix());
        }
        Manager::Render();

        Renderer::BeginSceneRender();
        Renderer::SetViewMatrix(Manager::GetEditorCamera()->GetViewMatrix());
        Renderer::SetProjectionMatrix(Manager::GetEditorCamera()->GetProjectionMatrix());
        Manager::Render();

        //ImGuiとウィンドウ全体のレンダリング
        Renderer::BeginFrame();

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

            if (Manager::IsShowSceneView()) {
                ImGui::Begin("Scene View", nullptr, ImGuiWindowFlags_NoScrollbar);
                {
                    ImVec2 viewportSize = ImGui::GetContentRegionAvail();
                    Renderer::ResizeSceneBuffer((UINT)viewportSize.x, (UINT)viewportSize.y);
                    void* myTexture = Renderer::GetSceneSRV();
                    if (myTexture) {
                        ImGui::Image(myTexture, viewportSize);
                        bool isHovered = ImGui::IsItemHovered();
                        Input::SetSceneViewHovered(isHovered);
                    }
                }
                ImGui::End();
            }

            if (Manager::IsShowGameView()) {
                ImGui::Begin("Game View", nullptr, ImGuiWindowFlags_NoScrollbar);//ゲームビュー描画
                {
                    ImVec2 viewportSize = ImGui::GetContentRegionAvail(); //描画領域のサイズ取得

                    //サイズが有効かつ現在のテクスチャサイズと異なる場合はリサイズ
					Renderer::ResizeGameBuffer((UINT)viewportSize.x, (UINT)viewportSize.y);

                    void* myTexture = Renderer::GetGameSRV();
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


        Renderer::EndFrame();

        Input::Update();    //Inputの更新

    }

    // クリーンアップ
	Manager::Finalize();
    ImGuiLayer::Shutdown();
    Renderer::Shutdown();
    UnregisterClass(wc.lpszClassName, hInstance);
    return 0;
}

