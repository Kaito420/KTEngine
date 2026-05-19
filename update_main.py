import re

def update_main():
    encodings = ['utf-8', 'cp932', 'shift_jis']
    content = None
    used_enc = None
    for enc in encodings:
        try:
            with open('src/main.cpp', 'r', encoding=enc) as f:
                content = f.read()
            used_enc = enc
            break
        except UnicodeDecodeError:
            pass
            
    if content is None:
        print("Failed to read main.cpp")
        return

    if 'ImGuizmo.h' not in content:
        content = content.replace('#include "ImGuiLayer.h"', '#include "ImGuiLayer.h"\n#include "libs/imgui/ImGuizmo.h"\n#include <DirectXMath.h>')

    if 'ImGuizmo::BeginFrame()' not in content:
        content = content.replace('ImGuiLayer::Begin();', 'ImGuiLayer::Begin();\n        ImGuizmo::BeginFrame();')

    old_scene_view = """                    void* myTexture = Renderer::GetSceneSRV();
                    if (myTexture) {
                        ImGui::Image(myTexture, viewportSize);
                        bool isHovered = ImGui::IsItemHovered();
                        Input::SetSceneViewHovered(isHovered);
                    }"""

    new_scene_view = """                    void* myTexture = Renderer::GetSceneSRV();
                    if (myTexture) {
                        ImGui::Image(myTexture, viewportSize);
                        bool isHovered = ImGui::IsItemHovered();
                        Input::SetSceneViewHovered(isHovered);
                        
                        auto selectedObj = Manager::GetCurrentScene()->GetSelectedGameObject();
                        if (selectedObj) {
                            ImGuizmo::SetOrthographic(false);
                            ImGuizmo::SetDrawlist();
                            
                            ImVec2 winPos = ImGui::GetWindowPos();
                            ImVec2 winMin = ImGui::GetWindowContentRegionMin();
                            ImVec2 winSize = ImGui::GetContentRegionAvail();
                            ImGuizmo::SetRect(winPos.x + winMin.x, winPos.y + winMin.y, winSize.x, winSize.y);
                            
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
                                selectedObj->_transform._scale = { sca[0], sca[1], sca[2] };
                            }
                        }
                    }"""

    if "ImGuizmo::Manipulate" not in content:
        content = content.replace(old_scene_view, new_scene_view)

    with open('src/main.cpp', 'w', encoding=used_enc) as f:
        f.write(content)
    
    print(f"Updated src/main.cpp using {used_enc} encoding")

update_main()
