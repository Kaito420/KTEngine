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

#include "RendererDX11.h"
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

void Manager::Initialize() {
	RegisterAllComponents();
	NewScene();
}

void Manager::Finalize() {

	//終了時にメモリ解放（セーブ処理はユーザーに委ねている）
	_editorScene.reset();
	_runtimeScene.reset();
}

void Manager::Update() {

	if (_mode == EngineMode::Editor) {
		if (_editorScene)
			_editorScene->UpdateEditor();
	}
	else if (_mode == EngineMode::Runtime) {
		if (_runtimeScene)
			_runtimeScene->Update();
	}
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

		if (ImGui::BeginMenu("Window")) { //Windowメニュー
			ImGui::MenuItem("Hierarchy", nullptr, &_showHierarchy);
			ImGui::MenuItem("Inspector", nullptr, &_showInspector);
			ImGui::MenuItem("Content Browser", nullptr, &_showContentBrowser);
			ImGui::MenuItem("Game View", nullptr, &_showGameView);
			
			ImGui::EndMenu();
		}
	ImGui::EndMainMenuBar();
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
	if (!_editorScene)return;

	//エディタシーンの状態をメモリ上にシリアライズ
	std::stringstream ss;
	{//一時保存なのでバイナリで高速に行う
		cereal::BinaryOutputArchive outArchive(ss);
		outArchive(_editorScene);
	}

	//メモリからデシリアライズしてランタイムシーンを生成
	{
		cereal::BinaryInputArchive inArchive(ss);
		inArchive(_runtimeScene);
	}

	if (_runtimeScene) {
		_mode = EngineMode::Runtime;
		_runtimeScene->OnLoaded(); //ID復元やPhysicsSystem再構築
		
	}

}

void Manager::Stop(){
	if (_mode == EngineMode::Editor)return;

	//現在のランタイムシーンを終了してEditorに戻す
	if (_runtimeScene) {
		_runtimeScene->Finalize();
		_runtimeScene.reset();
	}

	_mode = EngineMode::Editor;
}
