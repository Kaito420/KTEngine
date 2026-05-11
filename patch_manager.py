import codecs

# Patch main.cpp
with codecs.open(r'c:\Users\kaito\source\repos\KTEngine\src\main.cpp', 'r', 'cp932') as f:
    main_cpp = f.read()

if 'Manager::LoadEngineConfig();' not in main_cpp:
    main_cpp = main_cpp.replace('// DirectX11', 'Manager::LoadEngineConfig();\n    // DirectX11')
    with codecs.open(r'c:\Users\kaito\source\repos\KTEngine\src\main.cpp', 'w', 'cp932') as f:
        f.write(main_cpp)

# Patch Manager.h
with codecs.open(r'c:\Users\kaito\source\repos\KTEngine\src\Manager.h', 'r', 'cp932') as f:
    mgr_h = f.read()

if 'LoadEngineConfig' not in mgr_h:
    mgr_h = mgr_h.replace('static void Initialize();', 'static void LoadEngineConfig();\n\tstatic void SaveEngineConfig();\n\tstatic void Initialize();')
    with codecs.open(r'c:\Users\kaito\source\repos\KTEngine\src\Manager.h', 'w', 'cp932') as f:
        f.write(mgr_h)

# Patch Manager.cpp
with codecs.open(r'c:\Users\kaito\source\repos\KTEngine\src\Manager.cpp', 'r', 'cp932') as f:
    mgr_cpp = f.read()

if 'LoadEngineConfig' not in mgr_cpp:
    config_impl = '''
#include "Renderer.h"
#include <shellapi.h>

void Manager::LoadEngineConfig() {
    std::ifstream ifs("engine_config.json");
    if (ifs.is_open()) {
        try {
            cereal::JSONInputArchive iarchive(ifs);
            int api = 0;
            iarchive(cereal::make_nvp("GraphicsAPI", api));
            Renderer::SetGraphicsAPI(static_cast<GraphicsAPI>(api));
        } catch(...) {}
    }
}

void Manager::SaveEngineConfig() {
    std::ofstream ofs("engine_config.json");
    if (ofs.is_open()) {
        cereal::JSONOutputArchive oarchive(ofs);
        int api = static_cast<int>(Renderer::GetGraphicsAPI());
        oarchive(cereal::make_nvp("GraphicsAPI", api));
    }
}

'''
    mgr_cpp = mgr_cpp.replace('void Manager::Initialize() {', config_impl + 'void Manager::Initialize() {')

menu_bar = '''
            if (ImGui::BeginMenu("Options")) {
                GraphicsAPI currentAPI = Renderer::GetGraphicsAPI();
                int api_int = static_cast<int>(currentAPI);
                if (ImGui::Combo("Graphics API", &api_int, "DirectX 11\\0DirectX 12\\0")) {
                    if (api_int != static_cast<int>(currentAPI)) {
                        Renderer::SetGraphicsAPI(static_cast<GraphicsAPI>(api_int));
                        SaveEngineConfig();
                        ImGui::OpenPopup("Restarting Engine");
                    }
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginPopupModal("Restarting Engine", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::Text("Graphics API changed. Restarting application...");
                ImGui::Separator();
                
                static float restart_timer = 0.0f;
                restart_timer += ImGui::GetIO().DeltaTime;
                
                if (restart_timer > 2.0f) {
                    TCHAR szExeFileName[MAX_PATH];
                    GetModuleFileName(NULL, szExeFileName, MAX_PATH);
                    ShellExecute(NULL, "open", szExeFileName, NULL, NULL, SW_SHOWNORMAL);
                    PostQuitMessage(0);
                }
                ImGui::EndPopup();
            }
'''
if 'ImGui::BeginMenu("Options")' not in mgr_cpp:
    mgr_cpp = mgr_cpp.replace('if (ImGui::BeginMenu("Window")) {', menu_bar + '            if (ImGui::BeginMenu("Window")) {')
    with codecs.open(r'c:\Users\kaito\source\repos\KTEngine\src\Manager.cpp', 'w', 'cp932') as f:
        f.write(mgr_cpp)

print('Manager patched')
