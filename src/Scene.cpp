//=====================================================================================
// Scene.cpp
// Author:Kaito Aoki
// Date:2025/07/04
//=====================================================================================

#include "Scene.h"

void Scene::Initialize(){
	for (auto gameObject : _gameObjects) {
		gameObject->Awake();
	}
}

void Scene::Finalize() {
	for (auto gameObject : _gameObjects) {
		gameObject->OnDestroy();
	}
}

void Scene::Update(){
	for( auto gameObject : _gameObjects ){
		gameObject->Update();
	}

	for( auto gameObject : _gameObjects ){
		gameObject->LateUpdate();
	}
}

void Scene::Render(){
	for(auto gameObject : _gameObjects){
		gameObject->Render();
	}
}