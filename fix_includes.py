import re

def fix_includes():
    with open('src/main.cpp', 'r', encoding='utf-8') as f:
        content = f.read()

    # We need to move `#include "imgui.h"` above `#include "imgui/ImGuizmo.h"`
    # Or simply remove `#include "imgui/ImGuizmo.h"` and re-insert it after `#include "imgui.h"`
    
    # We remove both from the current position
    content = content.replace('#include "imgui/ImGuizmo.h"\n', '')
    content = content.replace('#include "imgui.h"\n', '')
    content = content.replace('#include "libs/imgui/ImGuizmo.h"\n', '')

    # Find where to put them
    # Put them after #include "ImGuiLayer.h"
    replacement = '#include "ImGuiLayer.h"\n#include "imgui.h"\n#include "libs/imgui/ImGuizmo.h"\n'
    content = content.replace('#include "ImGuiLayer.h"\n', replacement)

    with open('src/main.cpp', 'w', encoding='utf-8') as f:
        f.write(content)
        
fix_includes()
