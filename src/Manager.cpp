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

#include "SerializerRegistry.h"
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
EditorCamera Manager::_editorCamera;
bool Manager::_playPending = false;
bool Manager::_stopPending = false;

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
        } catch(...) {}
    }
}

void Manager::SaveEngineConfig(GraphicsAPI api) {
    std::ofstream ofs("engine_config.json");
    if (ofs.is_open()) {
        cereal::JSONOutputArchive oarchive(ofs);
        int api_val = static_cast<int>(api);
        oarchive(cereal::make_nvp("GraphicsAPI", api_val));
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
	_editorScene = std::make_shared<Scene>();
	_editorScene->Initialize();
	_currentScenePath = "";	//新規シーンはまだ保存されていないのでパスは空にしておく
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
                ImGui::EndMenu();
            }


            if (ImGui::BeginMenu("Window")) { //Windowメニュー
			ImGui::MenuItem("Hierarchy", nullptr, &_showHierarchy);
			ImGui::MenuItem("Inspector", nullptr, &_showInspector);
			ImGui::MenuItem("Content Browser", nullptr, &_showContentBrowser);
			ImGui::MenuItem("Game View", nullptr, &_showGameView);
			ImGui::MenuItem("Scene View", nullptr, &_showSceneView);
			
			ImGui::EndMenu();
		}
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
