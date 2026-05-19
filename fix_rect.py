with open('src/main.cpp', 'r', encoding='utf-8') as f:
    content = f.read()

old_rect = """                            ImVec2 winPos = ImGui::GetWindowPos();
                            ImVec2 winMin = ImGui::GetWindowContentRegionMin();
                            ImVec2 winSize = ImGui::GetContentRegionAvail();
                            ImGuizmo::SetRect(winPos.x + winMin.x, winPos.y + winMin.y, winSize.x, winSize.y);"""

new_rect = """                            ImGuizmo::SetRect(cursorPos.x, cursorPos.y, viewportSize.x, viewportSize.y);"""

if old_rect in content:
    content = content.replace(old_rect, new_rect)
    
    # We also need to add cursorPos capture
    old_viewport = """                    ImVec2 viewportSize = ImGui::GetContentRegionAvail();
                    Renderer::ResizeSceneBuffer((UINT)viewportSize.x, (UINT)viewportSize.y);"""
                    
    new_viewport = """                    ImVec2 viewportSize = ImGui::GetContentRegionAvail();
                    ImVec2 cursorPos = ImGui::GetCursorScreenPos();
                    Renderer::ResizeSceneBuffer((UINT)viewportSize.x, (UINT)viewportSize.y);"""
                    
    content = content.replace(old_viewport, new_viewport)

    with open('src/main.cpp', 'w', encoding='utf-8') as f:
        f.write(content)
    print("Fixed!")
else:
    print("Old rect not found.")
