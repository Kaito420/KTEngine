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

#include "RendererDX11.h"
#include "ImGuiLayer.h"
#include "imgui.h"

//std::shared_ptr<Scene> Manager::_currentScene = nullptr;
std::shared_ptr<Scene> Manager::_nextScene = nullptr;
std::shared_ptr<Scene> Manager::_editorScene = nullptr;
std::shared_ptr<Scene> Manager::_runtimeScene = nullptr;
EngineMode Manager::_mode = EngineMode::Editor;

void Manager::Initialize() {


	//とりあえずテスト作成
	//_currentScene = std::make_shared<SceneTitle>();

	//if(_currentScene) {
	//	_currentScene->Initialize();
	//}

	_editorScene = std::make_shared<SceneTitle>();
	if (_editorScene)
		_editorScene->Initialize();
}

void Manager::Finalize() {
	//if(_currentScene) {
	//	_currentScene->Finalize();
	//	_currentScene.reset();
	//}
	if (_editorScene) {
		_editorScene->Finalize();
		_editorScene.reset();
	}
}

void Manager::Update() {
	//if(_currentScene)
	//	_currentScene->Update();

	if (_mode == EngineMode::Editor) {
		if (_editorScene)
			_editorScene->Update();
	}
	else if (_mode == EngineMode::Runtime) {
		if (_runtimeScene)
			_runtimeScene->Update();
	}
}

void Manager::Render() {
	//if(_currentScene)
	//	_currentScene->Render();

	if (_mode == EngineMode::Editor) {
		if (_editorScene)
			_editorScene->Render();
	}
	else if (_mode == EngineMode::Runtime) {
		if (_runtimeScene)
			_runtimeScene->Render();

		if (_nextScene != nullptr) {
			_runtimeScene->Finalize();
			_runtimeScene = _nextScene;
			_runtimeScene->Initialize();
			_nextScene = nullptr;
		}
	}

}

std::shared_ptr<Scene> Manager::GetCurrentScene() {
	if (_mode == EngineMode::Editor)
		return _editorScene;
	else if (_mode == EngineMode::Runtime)
		return _runtimeScene;
}

void Manager::Play(){
	if (_mode == EngineMode::Runtime)return;

	//現在編集中のシーンをクローンしてランタイムへ
	_runtimeScene = _editorScene->Clone();
	_runtimeScene->Initialize();

	_mode = EngineMode::Runtime;
}

void Manager::Stop(){
	if (_mode == EngineMode::Editor)return;

	//現在のランタイムシーンを終了してEditorに戻す
	_runtimeScene->Finalize();
	_runtimeScene.reset();
	_mode = EngineMode::Editor;
}
