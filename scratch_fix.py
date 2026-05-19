with open('src/main.cpp', 'r', encoding='utf-8') as f:
    content = f.read()
content = content.replace('#include "libs/imgui/ImGuizmo.h"', '#include "imgui/ImGuizmo.h"')
with open('src/main.cpp', 'w', encoding='utf-8') as f:
    f.write(content)
