import re

def update_vcxproj():
    with open('KTEngine.vcxproj', 'r', encoding='utf-8') as f:
        content = f.read()
    
    content = re.sub(r'(<ClInclude Include="libs\\imgui\\imgui\.h" />)', r'\1\n    <ClInclude Include="libs\\imgui\\ImGuizmo.h" />', content)
    content = re.sub(r'(<ClCompile Include="libs\\imgui\\imgui\.cpp" />)', r'\1\n    <ClCompile Include="libs\\imgui\\ImGuizmo.cpp" />', content)
    
    with open('KTEngine.vcxproj', 'w', encoding='utf-8') as f:
        f.write(content)

def update_filters():
    with open('KTEngine.vcxproj.filters', 'r', encoding='utf-8') as f:
        content = f.read()
        
    clinclude = r"""(<ClInclude Include="libs\\imgui\\imgui\.h">[\s\S]*?</ClInclude>)"""
    new_clinclude = r'\1\n    <ClInclude Include="libs\\imgui\\ImGuizmo.h">\n      <Filter>ImGui</Filter>\n    </ClInclude>'
    content = re.sub(clinclude, new_clinclude, content)
    
    clcompile = r"""(<ClCompile Include="libs\\imgui\\imgui\.cpp">[\s\S]*?</ClCompile>)"""
    new_clcompile = r'\1\n    <ClCompile Include="libs\\imgui\\ImGuizmo.cpp">\n      <Filter>ImGui</Filter>\n    </ClCompile>'
    content = re.sub(clcompile, new_clcompile, content)

    with open('KTEngine.vcxproj.filters', 'w', encoding='utf-8') as f:
        f.write(content)

update_vcxproj()
update_filters()
