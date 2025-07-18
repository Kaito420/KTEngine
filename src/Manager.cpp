//=====================================================================================
// Manager.cpp
// Author:Kaito Aoki
// Date:2025/07/04
//=====================================================================================

#include <memory>
#include "Manager.h"
#include "Scene.h"

#include "RendererDX11.h"
#include "ImGuiLayer.h"

std::shared_ptr<Scene> Manager::_currentScene = nullptr;

void Manager::Initialize() {


	//‚Æ‚è‚ ‚¦‚¸ƒeƒXƒgì¬
	_currentScene = std::make_shared<Scene>();

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
}

std::shared_ptr<Scene> Manager::GetCurrentScene() {
	return _currentScene;
}