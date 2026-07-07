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
#include "PostProcessSystem.h"
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

        Manager::Update(); // Managerの更新

        // ゲームビューテクスチャにレンダリング
        if (Manager::IsShowGameView()) {
            Renderer::SetGeometryPass(true);
            Renderer::BeginGameRender();
            Camera* mainCamera = nullptr;
            if (Manager::GetCurrentScene()) {
                mainCamera = Manager::GetCurrentScene()->FindGameObjectByName<Camera>("Camera");
            }
            if (mainCamera) {
                Renderer::SetViewMatrix(mainCamera->GetViewMatrix());
                Renderer::SetProjectionMatrix(mainCamera->GetProjectionMatrix());
                Renderer::SetCameraPosition(XMFLOAT4(mainCamera->_transform._position.x, mainCamera->_transform._position.y, mainCamera->_transform._position.z, 1.0f));
            }
            Renderer::BindShaderConstantsDX12();
            Manager::Render();

            Renderer::SetGeometryPass(false);
            Renderer::ApplyDeferredLighting();
            Manager::Render();
            Renderer::ApplyPostProcess();
        }

        // シーンビューテクスチャにレンダリング
        if (Manager::IsShowSceneView()) {
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
            Renderer::ApplyPostProcess();
        }

        //ImGuiとウィンドウ全体のレンダリング
        Renderer::BeginFrame();

        ImGuiLayer::Begin();
        ImGuizmo::BeginFrame();
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
                    ImVec2 cursorPos = ImGui::GetCursorScreenPos();
                    Renderer::ResizeSceneBuffer((UINT)viewportSize.x, (UINT)viewportSize.y);
                    void* myTexture = Renderer::GetSceneSRV();
                    if (myTexture) {
                        ImGui::Image(myTexture, viewportSize);
                        bool isHovered = ImGui::IsItemHovered();
                        Input::SetSceneViewHovered(isHovered);
                        
                        auto selectedObj = Manager::GetCurrentScene()->GetSelectedGameObject();
                        if (selectedObj) {
                            ImGuizmo::SetOrthographic(false);
                            ImGuizmo::SetDrawlist();
                            
                            ImGuizmo::SetRect(cursorPos.x, cursorPos.y, viewportSize.x, viewportSize.y);
                            
                            EditorCamera* camera = Manager::GetEditorCamera();
                            DirectX::XMMATRIX view = camera->GetViewMatrix();
                            DirectX::XMMATRIX proj = camera->GetProjectionMatrix();
                            
                            float viewMat[16];
                            float projMat[16];
                            DirectX::XMStoreFloat4x4((DirectX::XMFLOAT4X4*)viewMat, view);
                            DirectX::XMStoreFloat4x4((DirectX::XMFLOAT4X4*)projMat, proj);
                            
                            float pos[3] = { selectedObj->_transform._position.x, selectedObj->_transform._position.y, selectedObj->_transform._position.z };
                            float rot[3] = { selectedObj->_transform._rotation.x, selectedObj->_transform._rotation.y, selectedObj->_transform._rotation.z };
                            float sca[3] = { selectedObj->_transform._scale.x, selectedObj->_transform._scale.y, selectedObj->_transform._scale.z };
                            
                            float matrix[16];
                            ImGuizmo::RecomposeMatrixFromComponents(pos, rot, sca, matrix);
                            
                            static ImGuizmo::OPERATION mCurrentGizmoOperation(ImGuizmo::TRANSLATE);
                            static ImGuizmo::MODE mCurrentGizmoMode(ImGuizmo::LOCAL);
                            
                            if (ImGui::IsKeyPressed(ImGuiKey_T)) mCurrentGizmoOperation = ImGuizmo::TRANSLATE;
                            if (ImGui::IsKeyPressed(ImGuiKey_E)) mCurrentGizmoOperation = ImGuizmo::ROTATE;
                            if (ImGui::IsKeyPressed(ImGuiKey_R)) mCurrentGizmoOperation = ImGuizmo::SCALE;
                            
                            ImGuizmo::Manipulate(viewMat, projMat, mCurrentGizmoOperation, mCurrentGizmoMode, matrix, NULL, NULL);
                            
                            if (ImGuizmo::IsUsing()) {
                                ImGuizmo::DecomposeMatrixToComponents(matrix, pos, rot, sca);
                                selectedObj->_transform._position = { pos[0], pos[1], pos[2] };
                                selectedObj->_transform._rotation = { rot[0], rot[1], rot[2] };
								selectedObj->_transform._quaternion = KTQUATERNION::FromEulerAngles(rot[0], rot[1], rot[2]);
                                selectedObj->_transform._scale = { sca[0], sca[1], sca[2] };
                            }
                        }
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

            // G-Buffer Visualizer Window
            {
                ImGui::Begin("G-Buffer Visualizer");
                static int viewGBufferType = 0; // 0: Scene, 1: Game
                ImGui::RadioButton("Scene View G-Buffer", &viewGBufferType, 0); ImGui::SameLine();
                ImGui::RadioButton("Game View G-Buffer", &viewGBufferType, 1);
                
                bool viewGameGBuffer = (viewGBufferType == 1);

                const char* names[6] = {
                    "Color (Base Color)", "Normal (World Space)", "Position (World Space)",
                    "Metallic", "Specular", "Roughness"
                };

                ImVec2 imgSize(240, 135); // 16:9 ratio

                ImGui::Columns(3, "gbuffer_columns", true);
                for (int i = 0; i < 6; i++) {
                    ImGui::Text("%s", names[i]);
                    void* texture = Renderer::GetGBufferSRV(i, viewGameGBuffer);
                    if (texture) {
                        ImGui::Image(texture, imgSize);
                    } else {
                        ImGui::Text("No Texture");
                    }
                    ImGui::NextColumn();
                    if (i == 2) {
                        ImGui::Separator();
                    }
                }
                ImGui::Columns(1);
                ImGui::End();
            }

            // Post Process Settings
            PostProcessSystem::RenderUI();
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

