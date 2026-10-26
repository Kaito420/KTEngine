#include <windows.h>
#include <time.h>
#include <cmath>
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
#include "PostProcessSystem.h"
#include "ProceduralTextureEditor.h"
#include "Profiler.h"
#include "../resource/resource.h"

// グローバル変数
HWND hwnd = nullptr;
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);                // Use ImGui::GetCurrentContext()
FileBrowser fileBrowser;

// ウィンドウプロシージャ
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg) {
    case WM_SIZE:
        if (wParam != SIZE_MINIMIZED) {
            UINT width = (UINT)LOWORD(lParam);
            UINT height = (UINT)HIWORD(lParam);
            Renderer::ResizeMainWindow(width, height);
        }
        return 0;

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
                      hInstance, LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1)), LoadCursor(NULL, IDC_ARROW), NULL, NULL,
                      "KTEngineClass", LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1)) };
    RegisterClassEx(&wc);
    hwnd = CreateWindow(wc.lpszClassName, "KTEngine",
        WS_OVERLAPPEDWINDOW, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT,
        NULL, NULL, wc.hInstance, NULL);

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    Manager::LoadEngineConfig();
    // DirectX12 初期化
    if (!Renderer::Init(hwnd)) return 1;

    // ImGui 初期化
    ImGuiLayer::Init(hwnd);

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

        Profiler::BeginFrame();

        Manager::Update(); // Managerの更新

        {
            PROFILE_SCOPE("Render");
            Renderer::BeginRenderFrame();

            // ゲームビューテクスチャにレンダリング
            if (Manager::IsShowGameView()) {
                // --- 0. シャドウマップ描画パス ---
                Renderer::SetShadowPass(true);
                Renderer::BeginShadowRender();
                Renderer::BindShaderConstantsDX12();
                Manager::Render();
                Renderer::EndShadowRender();
                Renderer::SetShadowPass(false);

                // --- 1. ジオメトリパス (G-Buffer生成) ---
                Renderer::SetGeometryPass(true);
                Renderer::BeginGameRender();
                Camera* mainCamera = nullptr;
                if (Manager::GetCurrentScene()) {
                    mainCamera = Manager::GetCurrentScene()->FindGameObjectByName<Camera>("Camera");
                }
                if (mainCamera) {
                    Renderer::SetViewMatrix(mainCamera->GetViewMatrix());
                    Renderer::SetProjectionMatrix(mainCamera->GetProjectionMatrix());
                    KTVECTOR3 camWorldPos = mainCamera->GetWorldPosition();
                    Renderer::SetCameraPosition(XMFLOAT4(camWorldPos.x, camWorldPos.y, camWorldPos.z, 1.0f));
                }
                Renderer::BindShaderConstantsDX12();
                Manager::Render();

                Renderer::SetGeometryPass(false);
                Renderer::ApplyDeferredLighting();
                Manager::Render();
                if (Manager::GetCurrentScene() && Manager::GetCurrentScene()->GetPhysicsSystem()) {
                    Manager::GetCurrentScene()->GetPhysicsSystem()->RenderDebug();
                }
                Renderer::ApplyPostProcess();
            }

            // シーンビューテクスチャにレンダリング
            if (Manager::IsShowSceneView()) {
                // --- 0. シャドウマップ描画パス ---
                Renderer::SetShadowPass(true);
                Renderer::BeginShadowRender();
                Renderer::BindShaderConstantsDX12();
                Manager::Render();
                Renderer::EndShadowRender();
                Renderer::SetShadowPass(false);

                // --- 1. ジオメトリパス (G-Buffer生成) ---
                Renderer::SetGeometryPass(true);
                Renderer::BeginSceneRender();
                Renderer::SetViewMatrix(Manager::GetEditorCamera()->GetViewMatrix());
                Renderer::SetProjectionMatrix(Manager::GetEditorCamera()->GetProjectionMatrix());
                {
                    XMMATRIX invView = XMMatrixInverse(nullptr, Manager::GetEditorCamera()->GetViewMatrix());
                    XMFLOAT4 editorCamPos;
                    XMStoreFloat4(&editorCamPos, invView.r[3]);
                    Renderer::SetCameraPosition(editorCamPos);
                }
                Renderer::BindShaderConstantsDX12();
                Manager::Render();

                Renderer::SetGeometryPass(false);
                Renderer::ApplyDeferredLighting();
                Manager::Render();
                if (Manager::GetCurrentScene() && Manager::GetCurrentScene()->GetPhysicsSystem()) {
                    Manager::GetCurrentScene()->GetPhysicsSystem()->RenderDebug();
                }
                Renderer::ApplyPostProcess();
            }
        }

        //ImGuiとウィンドウ全体のレンダリング
        Renderer::BeginFrame();

        {
            PROFILE_SCOPE("ImGui");
            ImGuiLayer::Begin();
            ImGuizmo::BeginFrame();
            {
                Manager::RenderMenuBar();
                Manager::RenderToolbar();
                if (Manager::IsShowContentBrowser())
                    fileBrowser.Render();
                if (Manager::IsShowHierarchy())
                    Manager::GetCurrentScene()->RenderHierarchy();
                if (Manager::IsShowInspector())
                    Manager::GetCurrentScene()->RenderInspector();


                if (Manager::IsShowProfiler()) {
                    Profiler::RenderUI(&Manager::GetShowProfiler());
                }

            if (Manager::IsShowSceneView()) {
                ImGui::Begin("Scene View", nullptr, ImGuiWindowFlags_NoScrollbar);
                {
                    Manager::RenderAspectRatioSelector();
                    ImGui::Separator();

                    ImVec2 availSize = ImGui::GetContentRegionAvail();
                    float targetAspect = Manager::GetAspectRatioValue();

                    if (availSize.x >= 1.0f && availSize.y >= 1.0f) {
                        float imgW = availSize.x;
                        float imgH = availSize.y;
                        if (availSize.x / availSize.y > targetAspect) {
                            imgH = floorf(availSize.y);
                            imgW = floorf(imgH * targetAspect);
                        } else {
                            imgW = floorf(availSize.x);
                            imgH = floorf(imgW / targetAspect);
                        }
                        if (imgW < 1.0f) imgW = 1.0f;
                        if (imgH < 1.0f) imgH = 1.0f;

                        float offsetX = (availSize.x - imgW) * 0.5f;
                        float offsetY = (availSize.y - imgH) * 0.5f;

                        ImVec2 currentCursor = ImGui::GetCursorPos();
                        ImGui::SetCursorPos(ImVec2(currentCursor.x + offsetX, currentCursor.y + offsetY));
                        ImVec2 imageScreenPos = ImGui::GetCursorScreenPos();

                        Renderer::ResizeSceneBuffer(imgW, imgH);
                        void* myTexture = Renderer::GetSceneSRV();
                        if (myTexture) {
                            ImGui::Image(myTexture, ImVec2(imgW, imgH));
                            bool isHovered = ImGui::IsItemHovered();
                            Input::SetSceneViewHovered(isHovered);
                            
                            auto selectedObj = Manager::GetCurrentScene()->GetSelectedGameObject();
                            if (selectedObj) {
                                ImGuizmo::SetOrthographic(false);
                                ImGuizmo::SetDrawlist();
                                
                                ImGuizmo::SetRect(imageScreenPos.x, imageScreenPos.y, imgW, imgH);
                            
                            EditorCamera* camera = Manager::GetEditorCamera();
                            DirectX::XMMATRIX view = camera->GetViewMatrix();
                            DirectX::XMMATRIX proj = camera->GetProjectionMatrix();
                            
                            float viewMat[16];
                            float projMat[16];
                            DirectX::XMStoreFloat4x4((DirectX::XMFLOAT4X4*)viewMat, view);
                            DirectX::XMStoreFloat4x4((DirectX::XMFLOAT4X4*)projMat, proj);
                            
                            float matrix[16];
                            DirectX::XMMATRIX worldMat = selectedObj->GetWorldMatrix();
                            DirectX::XMStoreFloat4x4((DirectX::XMFLOAT4X4*)matrix, worldMat);
                            
                            static ImGuizmo::OPERATION mCurrentGizmoOperation(ImGuizmo::TRANSLATE);
                            static ImGuizmo::MODE mCurrentGizmoMode(ImGuizmo::LOCAL);
                            
                            if (ImGui::IsKeyPressed(ImGuiKey_T)) mCurrentGizmoOperation = ImGuizmo::TRANSLATE;
                            if (ImGui::IsKeyPressed(ImGuiKey_E)) mCurrentGizmoOperation = ImGuizmo::ROTATE;
                            if (ImGui::IsKeyPressed(ImGuiKey_R)) mCurrentGizmoOperation = ImGuizmo::SCALE;
                            
                            ImGuizmo::Manipulate(viewMat, projMat, mCurrentGizmoOperation, mCurrentGizmoMode, matrix, NULL, NULL);
                            
                            if (ImGuizmo::IsUsing()) {
                                DirectX::XMMATRIX newWorldMat = DirectX::XMLoadFloat4x4((const DirectX::XMFLOAT4X4*)matrix);
                                selectedObj->SetWorldMatrix(newWorldMat);
                            }
                        }
                    }
                }
            }
            ImGui::End();
        }

            if (Manager::IsShowGameView()) {
                ImGui::Begin("Game View", nullptr, ImGuiWindowFlags_NoScrollbar);
                {
                    Manager::RenderAspectRatioSelector();
                    ImGui::Separator();

                    ImVec2 availSize = ImGui::GetContentRegionAvail();
                    float targetAspect = Manager::GetAspectRatioValue();

                    if (availSize.x >= 1.0f && availSize.y >= 1.0f) {
                        float imgW = availSize.x;
                        float imgH = availSize.y;
                        if (availSize.x / availSize.y > targetAspect) {
                            imgH = floorf(availSize.y);
                            imgW = floorf(imgH * targetAspect);
                        } else {
                            imgW = floorf(availSize.x);
                            imgH = floorf(imgW / targetAspect);
                        }
                        if (imgW < 1.0f) imgW = 1.0f;
                        if (imgH < 1.0f) imgH = 1.0f;

                        float offsetX = (availSize.x - imgW) * 0.5f;
                        float offsetY = (availSize.y - imgH) * 0.5f;

                        ImVec2 currentCursor = ImGui::GetCursorPos();
                        ImGui::SetCursorPos(ImVec2(currentCursor.x + offsetX, currentCursor.y + offsetY));

                        Renderer::ResizeGameBuffer(imgW, imgH);

                        void* myTexture = Renderer::GetGameSRV();
                        if (myTexture == nullptr)
                            ImGui::Text("Texture is NULL!");
                        else {
                            ImGui::Image(myTexture, ImVec2(imgW, imgH));
                            bool isHovered = ImGui::IsItemHovered();
                            Input::SetGameViewHovered(isHovered);
                        }
                    }
                }
                ImGui::End();
            }
            // G-Buffer Visualizer Window
            if (Manager::IsShowGBufferVisualizer()) {
                if (ImGui::Begin("G-Buffer Visualizer", &Manager::GetShowGBufferVisualizer())) {
                    static int viewGBufferType = 0; // 0: Scene, 1: Game
                    ImGui::RadioButton("Scene View G-Buffer", &viewGBufferType, 0); ImGui::SameLine();
                    ImGui::RadioButton("Game View G-Buffer", &viewGBufferType, 1);
                    
                    bool viewGameGBuffer = (viewGBufferType == 1);

                    const char* names[4] = {
                        "Color (Base Color)", "Normal (World Space)", "Position (World Space)",
                        "Material (ARM)"
                    };

                    ImVec2 imgSize(240, 135); // 16:9 ratio

                    ImGui::Columns(2, "gbuffer_columns", true);
                    for (int i = 0; i < 4; i++) {
                        ImGui::Text("%s", names[i]);
                        void* texture = Renderer::GetGBufferSRV(i, viewGameGBuffer);
                        if (texture) {
                            ImGui::Image(texture, imgSize);
                        } else {
                            ImGui::Text("No Texture");
                        }
                        ImGui::NextColumn();
                        if (i == 1) {
                            ImGui::Separator();
                        }
                    }
                    
                    // Shadow Map
                    ImGui::Text("Shadow Map");
                    void* shadowMap = Renderer::GetShadowMapSRV();
                    if (shadowMap) {
                        ImGui::Image(shadowMap, ImVec2(135, 135)); // 1:1 ratio square
                    } else {
                        ImGui::Text("No Texture");
                    }
                    ImGui::NextColumn();

                    ImGui::Columns(1);
                }
                ImGui::End();
            }

            // Post Process Settings Window
            if (Manager::IsShowPostProcess()) {
                PostProcessSystem::RenderUI(&Manager::GetShowPostProcess());
            }

            if (Manager::IsShowProceduralTextureEditor()) {
                ProceduralTextureEditor::RenderUI(&Manager::GetShowProceduralTextureEditor());
            }
        }
        ImGuiLayer::End();
        }

        Renderer::EndFrame();

        Input::Update();    //Inputの更新

        Profiler::EndFrame();
    }

    // クリーンアップ
	Manager::Finalize();
    ImGuiLayer::Shutdown();
    Renderer::Shutdown();
    UnregisterClass(wc.lpszClassName, hInstance);
    return 0;
}

