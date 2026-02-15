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

#include <fstream>
#include <sstream>
#include <cereal/archives/json.hpp>
#include <cereal/archives/binary.hpp>

std::shared_ptr<Scene> Manager::_nextScene = nullptr;
std::shared_ptr<Scene> Manager::_editorScene = nullptr;
std::shared_ptr<Scene> Manager::_runtimeScene = nullptr;
std::string Manager::_currentScenePath = "";
EngineMode Manager::_mode = EngineMode::Editor;

void Manager::Initialize() {
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
