//=====================================================================================
// Scene.cpp
// Author:Kaito Aoki
// Date:2025/07/04
//=====================================================================================

#include "Scene.h"
#include <imgui.h>
#include "Manager.h"
#include "GameObject.h"
#include "Shader.h"
#include "ShaderManager.h"

void Scene::Initialize(){
	//テスト用
	//============================================

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

	//削除処理
	_gameObjects.remove_if([](const std::shared_ptr<GameObject>& obj) { 
		if (obj->IsDestroy())
			obj->ProcessDestroyComponents();
		return obj->IsDestroy();
	});
}

void Scene::Render()const{
	for(const auto& gameObject : _gameObjects){
		if (gameObject->GetActive()) {
			const auto& shader = gameObject->GetComponent<Shader>();
			if (shader) {//shaderがある場合はそのシェーダーに変える
				ID3D11VertexShader* vs = ShaderManager::Instance().GetVertexShader(shader->GetVertexShaderID());
				ID3D11InputLayout* layout = ShaderManager::Instance().GetInputLayout(shader->GetVertexLayoutID());
				ID3D11PixelShader* ps = ShaderManager::Instance().GetPixelShader(shader->GetPixelShaderID());

				RendererDX11::GetContext()->VSSetShader(vs, nullptr, 0);
				RendererDX11::GetContext()->IASetInputLayout(layout);
				RendererDX11::GetContext()->PSSetShader(ps, nullptr, 0);
			}
			else {
				RendererDX11::SetDefaultShader();
			}
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
		for (auto& gameObject : _gameObjects) {
			if (_selectedObjId == gameObject->_id) {
				ImGui::Text("%s", gameObject->_name.c_str());
				ImGui::Checkbox("Active", &gameObject->_active);
				//Transform
				if(ImGui::BeginTable("Transform", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
				{
					bool changed = false;
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

					changed = false;
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::Text("Scale");
					for (int i = 0; i < 3; i++) {
						ImGui::TableSetColumnIndex(i + 1);
						ImGui::PushID(i + 4);
						ImGui::InputFloat("", &((&gameObject->_transform._scale.x)[i]));
						ImGui::PopID();
					}
					changed = false;
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::Text("Rotation");
					for (int i = 0; i < 3; i++) {
						ImGui::TableSetColumnIndex(i + 1);
						ImGui::PushID(i + 7);
						changed |= ImGui::InputFloat("", &((&gameObject->_transform._rotation.x)[i]));
						ImGui::PopID();
					}
					if (changed) {
						gameObject->_transform._quaternion = KTQUATERNION::FromEulerAngles(gameObject->_transform._rotation.x, gameObject->_transform._rotation.y, gameObject->_transform._rotation.z);
						if (gameObject->GetComponent<RigidBody>()) {
							gameObject->GetComponent<RigidBody>()->_orientation = gameObject->_transform._quaternion;
							gameObject->GetComponent<RigidBody>()->_angularVelocity = KTVECTOR3(0.0f, 0.0f, 0.0f);
						}
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

void Scene::RenderButton(){
	ImGui::SetNextWindowSize({ 720,50 });
	ImGui::SetWindowPos({ SCREEN_WIDTH / 2, -10 });
	ImGui::Begin("mode###MainButtonsBar", nullptr, ImGuiWindowFlags_NoMove);
	{
		if (ImGui::Button("Play")) {
			Manager::Play();
		}
		ImGui::SameLine();
		if (ImGui::Button("Stop")) {
			Manager::Stop();
		}
	}
	ImGui::End();
}

std::shared_ptr<Scene> Scene::Clone()const
{
	auto newScene = std::make_shared<Scene>();
	newScene->_physicsSystem = new PhysicsSystem();
	//GameObjectのコピー
	for (auto& gameObject : _gameObjects) {
		newScene->_gameObjects.push_back(gameObject);
	}
	return newScene;

}
