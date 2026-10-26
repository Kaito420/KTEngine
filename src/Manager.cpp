//=====================================================================================
// Manager.cpp
// Author:Kaito Aoki
// Date:2025/07/04
//=====================================================================================

#include <memory>
#include "Manager.h"
#include "Scene.h"
#include "SceneTitle.h"
#include "SceneGame.h"
#include "SceneResult.h"
#include "ScenePhysicsTest.h"

#include "Renderer.h"
#include "ImGuiLayer.h"
#include "imgui.h"
#include "imgui_internal.h"

#include "SerializerRegistry.h"
#include "ComponentRegistry.h"
#include "HotReloadManager.h"
#include "ComponentRegistry.h"
#include <commdlg.h>
#include <fstream>
#include <sstream>
#include <cereal/archives/json.hpp>
#include <cereal/archives/binary.hpp>

std::shared_ptr<Scene> Manager::_nextScene = nullptr;
std::shared_ptr<Scene> Manager::_editorScene = nullptr;
std::shared_ptr<Scene> Manager::_runtimeScene = nullptr;
std::string Manager::_currentScenePath = "";
EngineMode Manager::_mode = EngineMode::Editor;

//静的メンバの初期化
bool Manager::_showHierarchy = true;
bool Manager::_showInspector = true;
bool Manager::_showContentBrowser = true;
bool Manager::_showGameView = true;
bool Manager::_showSceneView = true;
bool Manager::_showPostProcess = true;
bool Manager::_showGBufferVisualizer = true;
bool Manager::_showProceduralTextureEditor = false;
bool Manager::_showProfiler = true;
bool Manager::_showColliderWireframe = false;
AspectRatioType Manager::_aspectRatio = AspectRatioType::Aspect_16_9;
EditorCamera Manager::_editorCamera;
bool Manager::_playPending = false;
bool Manager::_stopPending = false;
bool Manager::_newScenePending = false;
std::string Manager::_openScenePendingPath = "";

//====ヘルパー関数: Windowsのファイル保存ダイアログを開く====
std::string SaveFileDialog(const char* filter) {
	OPENFILENAMEA ofn;
	CHAR szFile[260] = { 0 };
	ZeroMemory(&ofn, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = GetActiveWindow(); // メインウィンドウのハンドル
	ofn.lpstrFile = szFile;
	ofn.nMaxFile = sizeof(szFile);
	ofn.lpstrFilter = filter; // 例:"Json File (*.json)\0*.json\0"
	ofn.nFilterIndex = 1;
	ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

	// デフォルトの拡張子
	ofn.lpstrDefExt = "json";

	if (GetSaveFileNameA(&ofn) == TRUE) {
		return std::string(ofn.lpstrFile);
	}
	return std::string(); //キャンセルされたら空文字
}


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

            int theme = 0;
            try {
                iarchive(cereal::make_nvp("Theme", theme));
                ImGuiLayer::SetTheme(static_cast<EditorTheme>(theme));
            } catch (...) {}

            int aspect = 0;
            try {
                iarchive(cereal::make_nvp("AspectRatio", aspect));
                _aspectRatio = static_cast<AspectRatioType>(aspect);
            } catch (...) {}
        } catch(...) {}
    }
}

void Manager::SaveEngineConfig(GraphicsAPI api) {
    std::ofstream ofs("engine_config.json");
    if (ofs.is_open()) {
        cereal::JSONOutputArchive oarchive(ofs);
        int api_val = static_cast<int>(api);
        oarchive(cereal::make_nvp("GraphicsAPI", api_val));
        int theme_val = static_cast<int>(ImGuiLayer::GetTheme());
        oarchive(cereal::make_nvp("Theme", theme_val));
        int aspect_val = static_cast<int>(_aspectRatio);
        oarchive(cereal::make_nvp("AspectRatio", aspect_val));
    }
}

void Manager::SaveEngineConfig() {
    SaveEngineConfig(Renderer::GetGraphicsAPI());
}

float Manager::GetAspectRatioValue() {
    switch (_aspectRatio) {
    case AspectRatioType::Aspect_16_10:
        return 16.0f / 10.0f;
    case AspectRatioType::Aspect_4_3:
        return 4.0f / 3.0f;
    case AspectRatioType::Aspect_16_9:
    default:
        return 16.0f / 9.0f;
    }
}

void Manager::RenderAspectRatioSelector() {
    int current = static_cast<int>(_aspectRatio);
    const char* items[] = { "16:9", "16:10", "4:3" };
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Aspect Ratio:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(90.0f);
    if (ImGui::Combo("##AspectRatioCombo", &current, items, IM_ARRAYSIZE(items))) {
        _aspectRatio = static_cast<AspectRatioType>(current);
        SaveEngineConfig();
    }
}

void Manager::Initialize() {
	RegisterAllComponents();
	NewScene();
}

void Manager::Finalize() {
	// 終了時にメモリ解放する前にGPUを待機
	Renderer::FlushGPUDX12();

	_editorScene.reset();
	_runtimeScene.reset();
}

void Manager::Update() {
	if (_stopPending) {
		_stopPending = false;
		// シーンを破棄する前にGPUの描画完了を待機
		Renderer::FlushGPUDX12();
		if (_runtimeScene) {
			_runtimeScene->Finalize();
			_runtimeScene.reset();
		}
		_mode = EngineMode::Editor;
	}

	if (_playPending) {
		_playPending = false;
		if (_editorScene) {
			std::stringstream ss;
			{
				cereal::BinaryOutputArchive outArchive(ss);
				outArchive(_editorScene);
			}
			{
				cereal::BinaryInputArchive inArchive(ss);
				inArchive(_runtimeScene);
			}
			if (_runtimeScene) {
				_mode = EngineMode::Runtime;
				_runtimeScene->OnLoaded();
			}
		}
	}

	if (_newScenePending) {
		_newScenePending = false;
		Renderer::FlushGPUDX12();
		_editorScene = std::make_shared<Scene>();
		_editorScene->Initialize();
		_currentScenePath = "";
	}

	if (!_openScenePendingPath.empty()) {
		std::string path = _openScenePendingPath;
		_openScenePendingPath.clear();
		Renderer::FlushGPUDX12();
		_OpenSceneInternal(path);
	}

	if (_mode == EngineMode::Editor) {
		if (_editorScene)
			_editorScene->UpdateEditor();
	}
	else if (_mode == EngineMode::Runtime) {
		if (_runtimeScene)
			_runtimeScene->Update();
	}

	// EditorCamera is updated regardless of the mode so it can be used in the Scene View during play mode
	_editorCamera.Update();
}

void Manager::Render() {

	if (_mode == EngineMode::Editor) {
		if (_editorScene)
			_editorScene->Render();
	}
	else if (_mode == EngineMode::Runtime) {
		if (_runtimeScene)
			_runtimeScene->Render();

		//if (_nextScene != nullptr) {
		//	_runtimeScene->Finalize();
		//	_runtimeScene = _nextScene;
		//	_runtimeScene->Initialize();
		//	_nextScene = nullptr;
		//}
		//シーン切り替えはjsonからOpenSceneする形に変更
	}

}

std::shared_ptr<Scene> Manager::GetCurrentScene() {
	if (_mode == EngineMode::Editor)
		return _editorScene;
	else if (_mode == EngineMode::Runtime)
		return _runtimeScene;
}

void Manager::NewScene(){
	_newScenePending = true;
}

void Manager::SaveScene(const std::string& filePath){
	if (!_editorScene)return;

	std::ofstream os(filePath);
	if (os.is_open()) {//Sceneタグでスマートポインタ毎保存（ポリモーフィズムのため）
		cereal::JSONOutputArchive archive(os);
		archive(cereal::make_nvp("Scene", _editorScene));

		_currentScenePath = filePath; //保存したファイルパスを現在のシーンパスとして保存
	}
}

void Manager::OpenScene(const std::string& filePath){
	_openScenePendingPath = filePath;
}

void Manager::_OpenSceneInternal(const std::string& filePath){
	std::ifstream is(filePath);
	if (is.is_open()) {
		try {
			cereal::JSONInputArchive archive(is);

			//一旦読み込み用のシーン
			std::shared_ptr<Scene> loadedScene;
			archive(cereal::make_nvp("Scene", loadedScene));

			if (loadedScene) {//読み込み成功時にエディタシーンに書き換え
				_editorScene = loadedScene;
				_editorScene->OnLoaded();//physicsSystemの初期化など
				_currentScenePath = filePath; //読み込んだファイルパスを現在のシーンパスとして保存
			}
		}
		catch (const std::exception& e) {
			//読み込み失敗
			MessageBoxA(NULL, e.what(), "Scene Load Error", MB_OK | MB_ICONERROR);
		}
	}
}

void Manager::RenderMenuBar(){
	static bool bShowRestartPopup = false;
	if (ImGui::BeginMainMenuBar()) {
		if (ImGui::BeginMenu("File")) {//Fileメニュー

			if (ImGui::MenuItem("New Scene")) {
				NewScene();
			}
			
			if (ImGui::MenuItem("Open Scene")) {
				//ファイルダイアログを開いてシーンを選択

			}

			ImGui::Separator();

			if (ImGui::MenuItem("Save Scene")) {
				SaveScene();
			}

			if (ImGui::MenuItem("Save Scene As")) {
				SaveSceneAs();
			}

			if (ImGui::MenuItem("Exit")) {
				PostQuitMessage(0);
			}
		ImGui::EndMenu();
		}

		
            if (ImGui::BeginMenu("Options")) {
                GraphicsAPI currentAPI = Renderer::GetGraphicsAPI();
                int api_int = static_cast<int>(currentAPI);
                if (ImGui::Combo("Graphics API", &api_int, "DirectX 11\0DirectX 12\0")) {
                    if (api_int != static_cast<int>(currentAPI)) {
                        SaveEngineConfig(static_cast<GraphicsAPI>(api_int));
                        bShowRestartPopup = true;
                    }
                }
                ImGui::Separator();
                if (ImGui::BeginMenu("Theme")) {
                    EditorTheme currentTheme = ImGuiLayer::GetTheme();
                    if (ImGui::MenuItem("Modern Engine (UE5 / Rider)", nullptr, currentTheme == EditorTheme::ModernEngine)) {
                        ImGuiLayer::SetTheme(EditorTheme::ModernEngine);
                        SaveEngineConfig();
                    }
                    if (ImGui::MenuItem("Cyber Slate (VSCode / Neon)", nullptr, currentTheme == EditorTheme::CyberSlate)) {
                        ImGuiLayer::SetTheme(EditorTheme::CyberSlate);
                        SaveEngineConfig();
                    }
                    if (ImGui::MenuItem("Nord / Catppuccin (Soft Pastel)", nullptr, currentTheme == EditorTheme::Catppuccin)) {
                        ImGuiLayer::SetTheme(EditorTheme::Catppuccin);
                        SaveEngineConfig();
                    }
                    ImGui::Separator();
                    if (ImGui::MenuItem("Classic Dark (ImGui Default)", nullptr, currentTheme == EditorTheme::ClassicDark)) {
                        ImGuiLayer::SetTheme(EditorTheme::ClassicDark);
                        SaveEngineConfig();
                    }
                    ImGui::EndMenu();
                }
                ImGui::Separator();
                if (ImGui::BeginMenu("Aspect Ratio")) {
                    int current = static_cast<int>(_aspectRatio);
                    if (ImGui::MenuItem("16:9", nullptr, current == 0)) {
                        _aspectRatio = AspectRatioType::Aspect_16_9;
                        SaveEngineConfig();
                    }
                    if (ImGui::MenuItem("16:10", nullptr, current == 1)) {
                        _aspectRatio = AspectRatioType::Aspect_16_10;
                        SaveEngineConfig();
                    }
                    if (ImGui::MenuItem("4:3", nullptr, current == 2)) {
                        _aspectRatio = AspectRatioType::Aspect_4_3;
                        SaveEngineConfig();
                    }
                    ImGui::EndMenu();
                }
                ImGui::Separator();
                ImGui::MenuItem("Show Collider Wireframes", nullptr, &_showColliderWireframe);
                ImGui::EndMenu();
            }


            if (ImGui::BeginMenu("Window")) { //Windowメニュー
			ImGui::MenuItem("Hierarchy", nullptr, &_showHierarchy);
			ImGui::MenuItem("Inspector", nullptr, &_showInspector);
			ImGui::MenuItem("Content Browser", nullptr, &_showContentBrowser);
			ImGui::MenuItem("Game View", nullptr, &_showGameView);
			ImGui::MenuItem("Scene View", nullptr, &_showSceneView);
			ImGui::Separator();
			ImGui::MenuItem("Post Process", nullptr, &_showPostProcess);
			ImGui::MenuItem("G-Buffer Visualizer", nullptr, &_showGBufferVisualizer);
			ImGui::MenuItem("Procedural Texture Editor", nullptr, &_showProceduralTextureEditor);
			ImGui::Separator();
			ImGui::MenuItem("Profiler", nullptr, &_showProfiler);
			ImGui::Separator();
			if (ImGui::MenuItem("Reset Window Layout")) {
				ImGuiLayer::ResetLayout();
			}
			
			ImGui::EndMenu();
		}
		HotReloadManager::RenderMenuItems();
	ImGui::EndMainMenuBar();
	}

    if (bShowRestartPopup) {
        ImGui::OpenPopup("Restarting Engine");
        bShowRestartPopup = false;
    }

    if (ImGui::BeginPopupModal("Restarting Engine", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Graphics API changed. Restarting application...");
        ImGui::Separator();
        
        static float restart_timer = 0.0f;
        restart_timer += ImGui::GetIO().DeltaTime;
        
        if (restart_timer > 1.5f) {
            TCHAR szExeFileName[MAX_PATH];
            GetModuleFileName(NULL, szExeFileName, MAX_PATH);
            ShellExecute(NULL, "open", szExeFileName, NULL, NULL, SW_SHOWNORMAL);
            PostQuitMessage(0);
        }
        ImGui::EndPopup();
    }
}

void Manager::SaveScene(){
	if (!_currentScenePath.empty()) {
		SaveScene(_currentScenePath);
	}
	else {
		SaveSceneAs();
	}
}

void Manager::SaveSceneAs(){
	std::string filePath = SaveFileDialog("Scene File (*.json)\0*.json\0All Files (*.*)\0*.*\0");
	
	if (!filePath.empty()) {
		SaveScene(filePath);
	}
}

void Manager::Play(){
	if (_mode == EngineMode::Runtime)return;
	_playPending = true;
}

void Manager::Stop(){
	if (_mode == EngineMode::Editor)return;
	_stopPending = true;
}

void Manager::RenderToolbar() {
	ImGuiViewport* viewport = ImGui::GetMainViewport();
	const float toolbarHeight = 36.0f;
	const float btnWidth = 72.0f;
	const float btnHeight = 24.0f;
	const float spacing = 6.0f;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 0.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);

	if (ImGui::BeginViewportSideBar("##MainToolbar", viewport, ImGuiDir_Up, toolbarHeight,
		ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings)) {

		bool isPlaying = (_mode == EngineMode::Runtime);
		float availWidth = ImGui::GetContentRegionAvail().x;
		float totalWidth = (btnWidth * 3.0f) + (spacing * 2.0f);
		float startX = (availWidth - totalWidth) * 0.5f;
		float startY = (toolbarHeight - btnHeight) * 0.5f;

		ImGui::SetCursorPos(ImVec2(startX > 0.0f ? startX : 8.0f, startY));

		// --- Play Button ---
		if (isPlaying) {
			// Green highlight while playing
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.65f, 0.32f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.24f, 0.78f, 0.38f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.14f, 0.55f, 0.26f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
		}

		if (ImGui::Button(isPlaying ? "Playing##Play" : "Play", ImVec2(btnWidth, btnHeight))) {
			if (isPlaying) {
				Stop();
			} else {
				Play();
			}
		}

		if (isPlaying) {
			ImGui::PopStyleColor(4);
		}

		ImGui::SameLine(0.0f, spacing);

		// --- Stop Button ---
		if (!isPlaying) {
			// Disabled while in editor mode
			ImGui::BeginDisabled(true);
		} else {
			// Red hover highlight while playing
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.85f, 0.25f, 0.25f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.70f, 0.20f, 0.20f, 1.0f));
		}

		if (ImGui::Button("Stop", ImVec2(btnWidth, btnHeight))) {
			Stop();
		}

		if (isPlaying) {
			ImGui::PopStyleColor(2);
		} else {
			ImGui::EndDisabled();
		}

		ImGui::SameLine(0.0f, spacing);

		// --- Reload Button ---
		bool isCompiling = HotReloadManager::IsCompilingCpp();
		if (isCompiling) {
			ImGui::BeginDisabled(true);
		}
		if (ImGui::Button(isCompiling ? "Building..." : "Reload", ImVec2(btnWidth + 8.0f, btnHeight))) {
			HotReloadManager::CheckAndReloadModified();
		}
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
			ImGui::SetTooltip("Hot-Reload Modified HLSL / C++ Files (Ctrl+R)");
		}
		if (isCompiling) {
			ImGui::EndDisabled();
		}

		ImGui::End();
	}
	ImGui::PopStyleVar(2);
}

bool Manager::SaveHotReloadSnapshot(const std::string& filePath, HWND hwnd) {
	try {
		std::ofstream ofs(filePath, std::ios::binary);
		if (!ofs.is_open()) return false;

		cereal::BinaryOutputArchive ar(ofs);

		int modeInt = static_cast<int>(_mode);
		int aspectInt = static_cast<int>(_aspectRatio);
		ar(modeInt, _currentScenePath, aspectInt);
		ar(_showHierarchy, _showInspector, _showContentBrowser, _showGameView, _showSceneView);
		ar(_showPostProcess, _showGBufferVisualizer, _showProceduralTextureEditor, _showProfiler, _showColliderWireframe);

		DirectX::XMFLOAT3 camPos = _editorCamera.GetPosition();
		DirectX::XMFLOAT3 camRot = _editorCamera.GetRotation();
		float camDist = _editorCamera.GetDistance();
		ar(camPos.x, camPos.y, camPos.z, camRot.x, camRot.y, camRot.z, camDist);

		BOOL isZoomed = (hwnd != nullptr) ? IsZoomed(hwnd) : FALSE;
		RECT winRect = { 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT };
		if (hwnd != nullptr) {
			GetWindowRect(hwnd, &winRect);
		}
		ar(isZoomed, winRect.left, winRect.top, winRect.right, winRect.bottom);

		bool hasEditorScene = (_editorScene != nullptr);
		ar(hasEditorScene);
		if (hasEditorScene) {
			ar(_editorScene);
			int selId = _editorScene->GetSelectedObjId();
			ar(selId);
		}

		bool hasRuntimeScene = (_runtimeScene != nullptr);
		ar(hasRuntimeScene);
		if (hasRuntimeScene) {
			ar(_runtimeScene);
			int selId = _runtimeScene->GetSelectedObjId();
			ar(selId);
		}

		return true;
	}
	catch (...) {
		return false;
	}
}

bool Manager::LoadHotReloadSnapshot(const std::string& filePath, HWND hwnd) {
	try {
		std::ifstream ifs(filePath, std::ios::binary);
		if (!ifs.is_open()) return false;

		Renderer::FlushGPUDX12();

		cereal::BinaryInputArchive ar(ifs);

		int modeInt = 0;
		int aspectInt = 0;
		ar(modeInt, _currentScenePath, aspectInt);
		ar(_showHierarchy, _showInspector, _showContentBrowser, _showGameView, _showSceneView);
		ar(_showPostProcess, _showGBufferVisualizer, _showProceduralTextureEditor, _showProfiler, _showColliderWireframe);

		_aspectRatio = static_cast<AspectRatioType>(aspectInt);

		DirectX::XMFLOAT3 camPos = {};
		DirectX::XMFLOAT3 camRot = {};
		float camDist = 100.0f;
		ar(camPos.x, camPos.y, camPos.z, camRot.x, camRot.y, camRot.z, camDist);
		_editorCamera.SetPosition(camPos);
		_editorCamera.SetRotation(camRot);
		_editorCamera.SetDistance(camDist);
		_editorCamera.Update();

		BOOL isZoomed = FALSE;
		LONG left = 0, top = 0, right = SCREEN_WIDTH, bottom = SCREEN_HEIGHT;
		ar(isZoomed, left, top, right, bottom);
		if (hwnd != nullptr) {
			if (isZoomed) {
				ShowWindow(hwnd, SW_SHOWMAXIMIZED);
			} else if (right > left && bottom > top) {
				SetWindowPos(hwnd, NULL, left, top, right - left, bottom - top, SWP_NOZORDER | SWP_NOACTIVATE);
			}
		}

		bool hasEditorScene = false;
		ar(hasEditorScene);
		if (hasEditorScene) {
			std::shared_ptr<Scene> loadedEditorScene;
			int selId = -1;
			ar(loadedEditorScene);
			ar(selId);
			if (loadedEditorScene) {
				_editorScene = loadedEditorScene;
				_editorScene->OnLoaded();
				_editorScene->SetSelectedObjId(selId);
			}
		}

		bool hasRuntimeScene = false;
		ar(hasRuntimeScene);
		if (hasRuntimeScene) {
			std::shared_ptr<Scene> loadedRuntimeScene;
			int selId = -1;
			ar(loadedRuntimeScene);
			ar(selId);
			if (loadedRuntimeScene) {
				_runtimeScene = loadedRuntimeScene;
				_runtimeScene->OnLoaded();
				_runtimeScene->SetSelectedObjId(selId);
			}
		}

		_mode = static_cast<EngineMode>(modeInt);
		_newScenePending = false;
		_playPending = false;
		_stopPending = false;

		return true;
	}
	catch (...) {
		return false;
	}
}