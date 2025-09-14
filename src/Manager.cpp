//=====================================================================================
// Manager.cpp
// Author:Kaito Aoki
// Date:2025/07/04
//=====================================================================================

#include <memory>
#include "Manager.h"
#include "Scene.h"
#include "SceneGame.h"

#include "RendererDX11.h"
#include "ImGuiLayer.h"
#include "imgui.h"

std::shared_ptr<Scene> Manager::_currentScene = nullptr;
std::shared_ptr<Scene> Manager::_nextScene = nullptr;

void Manager::Initialize() {


	//Ç∆ÇËÇ†Ç¶Ç∏ÉeÉXÉgçÏê¨
	_currentScene = std::make_shared<SceneGame>();

	if(_currentScene) {
		_currentScene->Initialize();
	}
}

void Manager::Finalize() {
	if(_currentScene) {
		_currentScene->Finalize();
		_currentScene.reset();
	}
}

void Manager::Update() {
	if(_currentScene)
		_currentScene->Update();
}

void Manager::Render() {
	if(_currentScene)
		_currentScene->Render();

	if (_nextScene != nullptr) {
		_currentScene->Finalize();
		_currentScene = _nextScene;
		_currentScene->Initialize();
		_nextScene = nullptr;
	}
}

std::shared_ptr<Scene> Manager::GetCurrentScene() {
	return _currentScene;
}