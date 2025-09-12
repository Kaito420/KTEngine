//=====================================================================================
// Scene.cpp
// Author:Kaito Aoki
// Date:2025/07/04
//=====================================================================================

#include "Scene.h"
#include <imgui.h>

#include "Camera.h"
#include "Square.h"
#include "Sphere.h"
#include "Cube.h"
#include "Collider.h"
#include "RigidBody.h"

void Scene::Initialize(){
	//テスト用
	//============================================

	_physicsSystem = new PhysicsSystem();

	AddGameObject<Camera>()->_name = "Camera";
	AddGameObject<GameObject>()->_name = "Square1";
	FindGameObjectByName<GameObject>("Square1")->AddComponent<Square>();
	AddGameObject<GameObject>()->_name = "Sphere1";
	FindGameObjectByName<GameObject>("Sphere1")->AddComponent<Sphere>();
	AddGameObject<GameObject>()->_name = "Cube1";
	FindGameObjectByName<GameObject>("Cube1")->AddComponent<Cube>();
	FindGameObjectByName<GameObject>("Cube1")->AddComponent<ColliderBox>();
	FindGameObjectByName<GameObject>("Cube1")->AddComponent<RigidBody>();


	AddGameObject<GameObject>()->_name = "Cube2";
	FindGameObjectByName<GameObject>("Cube2")->_transform._position = { 0.0f,1.5f,0.0f };
	FindGameObjectByName<GameObject>("Cube2")->AddComponent<Cube>();
	FindGameObjectByName<GameObject>("Cube2")->AddComponent<ColliderBox>();
	FindGameObjectByName<GameObject>("Cube2")->AddComponent<RigidBody>();



	//============================================
}

void Scene::Finalize() {
	for (auto& gameObject : _gameObjects) {
		gameObject->OnDestroy();
	}
}

void Scene::Update(){
	for( auto& gameObject : _gameObjects ){
		if(gameObject->GetActive() && gameObject->GetStarted() == false) {
			gameObject->Start();
			gameObject->Started();
		}
		if (gameObject->GetActive()) {
			gameObject->Update();
			gameObject->UpdateComponents();
		}
	}

	_physicsSystem->Update();

	for( auto& gameObject : _gameObjects ){
		if (gameObject->GetActive()) {
			gameObject->LateUpdate();
			gameObject->LateUpdateComponents();
		}
	}
}

void Scene::Render()const{
	for(const auto& gameObject : _gameObjects){
		if (gameObject->GetActive()) {
			gameObject->Render();
			gameObject->RenderComponents();
		}
	}
}

void Scene::RenderHierarchy()
{
	ImGui::Begin("Hierarchy View"); 
	{
		int index = 0;
		for (auto it = _gameObjects.begin(); it != _gameObjects.end(); it++, index++) {

			auto& gameObject = *it;
			//選択処理
			bool isSelected = (_selectedObjId == gameObject->_id);
			if (ImGui::Selectable(gameObject->_name.c_str(), isSelected))
				_selectedObjId = gameObject->_id;
			// --- Drag Source ---
			if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
				GameObject* srcPtr = gameObject.get(); // shared_ptrの中身の生ポインタ
				ImGui::SetDragDropPayload("DND_GAMEOBJECT", &srcPtr, sizeof(srcPtr));
				ImGui::Text("%s", gameObject->_name.c_str());
				ImGui::EndDragDropSource();
			}

			// --- Drag Target ---
			if (ImGui::BeginDragDropTarget()) {
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DND_GAMEOBJECT")) {
					GameObject* srcPtr = *(GameObject**)payload->Data;

					// find source iterator by matching pointer (safe & robust)
					auto srcIt = std::find_if(_gameObjects.begin(), _gameObjects.end(),
						[&](const std::shared_ptr<GameObject>& p) { return p.get() == srcPtr; });

					if (srcIt != _gameObjects.end() && srcIt != it) {
						//src が target の前にあるかを調べる（srcBeforeTarget が true なら前）
						bool srcBeforeTarget = false;
						for (auto t = srcIt; t != _gameObjects.end(); ++t) {
							if (t == it) { 
								srcBeforeTarget = true;
								break;
							}
						}

						// 挿入位置を決める（前なら target の次に、後なら target の前に）
						auto insertPos = it;
						if (srcBeforeTarget) ++insertPos; // target の後ろに入れる

						// ノードを移動（O(1)）
						_gameObjects.splice(insertPos, _gameObjects, srcIt);
					}
				}
				ImGui::EndDragDropTarget();
			}
		}
	}
	ImGui::End();

}

void Scene::RenderInspector()
{
	ImGui::Begin("Inspector");
	{
		for (auto gameObject : _gameObjects) {
			if (_selectedObjId == gameObject->_id) {
				ImGui::Text("%s", gameObject->_name.c_str());
				ImGui::Checkbox("Active", &gameObject->_active);
				//Transform
				if(ImGui::BeginTable("Transform", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
				{

					ImGui::TableSetupColumn("Property");
					ImGui::TableSetupColumn("X");
					ImGui::TableSetupColumn("Y");
					ImGui::TableSetupColumn("Z");
					ImGui::TableHeadersRow();


					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::Text("Position");
					for (int i = 0; i < 3; i++) {
						ImGui::TableSetColumnIndex(i + 1);
						ImGui::PushID(i);
						ImGui::InputFloat("", &((&gameObject->_transform._position.x)[i]));
						ImGui::PopID();
					}


					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::Text("Scale");
					for (int i = 0; i < 3; i++) {
						ImGui::TableSetColumnIndex(i + 1);
						ImGui::PushID(i + 4);
						ImGui::InputFloat("", &((&gameObject->_transform._scale.x)[i]));
						ImGui::PopID();
					}

					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::Text("Rotation");
					for (int i = 0; i < 3; i++) {
						ImGui::TableSetColumnIndex(i + 1);
						ImGui::PushID(i + 7);
						ImGui::InputFloat("", &((&gameObject->_transform._rotation.x)[i]));
						ImGui::PopID();
					}
					ImGui::EndTable();
				}

				for (auto& component : gameObject->_components) {
					if(ImGui::TreeNode(component->GetComponentName().c_str())){
						ImGui::Checkbox("Active", &component->_active);
						component->ShowUI();
						ImGui::TreePop();
					}
				}
			}
		}
	}
	ImGui::End();
}
