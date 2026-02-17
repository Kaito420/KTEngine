//=====================================================================================
// Scene.cpp
// Author:Kaito Aoki
// Date:2025/07/04
//=====================================================================================

#include "ComponentRegistry.h"
#include "Scene.h"
#include <imgui.h>
#include "Manager.h"
#include "GameObject.h"
#include "Shader.h"
#include "ShaderManager.h"
#include "Camera.h"
#include "Sky.h"
#include "Light.h"
#include "CubeObject.h"
#include "SphereObject.h"

std::string Scene::GenerateUniqueName(const std::string& baseName){
	std::string uniqueName = baseName;
	int index = 1;

	//重複している名前がある限りループしてインデックスを進める
	while (FindGameObjectByName<GameObject>(uniqueName) != nullptr) {
		std::stringstream ss;
		ss << baseName << "(" << index << ")";
		uniqueName = ss.str();
		index++;
	}
	return uniqueName;
}

void Scene::Initialize(){
	_physicsSystem = new PhysicsSystem();

	AddGameObject<Camera>()->_name = "Camera";
	AddGameObject<Sky>()->_name = "Sky";
	AddGameObject<Light>()->_name = "Light";
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

void Scene::UpdateEditor() {
	//エディタモードでも実行するオブジェクトの更新
	//(カメラやスカイドームライトなど、物理システムは更新しない）
	for (auto& gameObject : _gameObjects) {
		if (gameObject->GetActive()) {
			gameObject->UpdateEditor();
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

void Scene::OnLoaded() {
	//PhysicsSystemの再生成
	if(_physicsSystem)
		delete _physicsSystem;
	_physicsSystem = new PhysicsSystem();
	uint32_t maxId = 0;
	for(auto& gameObject: _gameObjects) {
		gameObject->OnLoaded();
		//idの最大値を更新
		if(maxId < gameObject->_id)
			maxId = gameObject->_id;
	}
	GameObject::_nextId = maxId + 1; //次のIDを最大値+1に設定
}

void Scene::RenderHierarchy()
{
	ImGui::Begin("Hierarchy View"); 
	{
		int index = 0;
		for (auto it = _gameObjects.begin(); it != _gameObjects.end(); it++, index++) {

			auto& gameObject = *it;

			ImGui::PushID(gameObject->_id); //一意なIDを設定
			//選択処理
			bool isSelected = (_selectedObjId == gameObject->_id);
			if (ImGui::Selectable(gameObject->_name.c_str(), isSelected))
				_selectedObjId = gameObject->_id;
			//Drag Source
			if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
				GameObject* srcPtr = gameObject.get(); // shared_ptrの中身の生ポインタ
				ImGui::SetDragDropPayload("DND_GAMEOBJECT", &srcPtr, sizeof(GameObject*));
				ImGui::Text("%s", gameObject->_name.c_str());
				ImGui::EndDragDropSource();
			}

			//Drag & Drop Target
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

			//右クリックでポップアップ
			if (ImGui::BeginPopupContextItem()) {
				//リネーム
				if (ImGui::MenuItem("Rename")) {
					_renameTargetId = gameObject->_id;
					//リネーム対象の名前をバッファにコピー
					strcpy_s(_renameBuffer, sizeof(_renameBuffer), gameObject->_name.c_str());
					_openPopup = true;	//ポップアップを開く合図
				}
				if (ImGui::MenuItem("Delete")) {
					gameObject->Destroy();
				}
				ImGui::EndPopup();
			}
			ImGui::PopID();
		}
		//何もない場所での右クリックメニュー
		if (ImGui::BeginPopupContextWindow(nullptr, 
			ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
			if(ImGui::MenuItem("Create Empty")){
				GameObject* newObj = AddGameObject<GameObject>();
				newObj->_name = GenerateUniqueName("GameObject");
				_selectedObjId = newObj->_id; //新規作成したオブジェクトを選択
			}
			if (ImGui::MenuItem("Cube")) {
				GameObject* newObj = AddGameObject<CubeObject>();
				newObj->_name = GenerateUniqueName("Cube");
				_selectedObjId = newObj->_id;
			}
			if (ImGui::MenuItem("Sphere")) {
				GameObject* newObj = AddGameObject<SphereObject>();
				newObj->_name = GenerateUniqueName("Sphere");
				_selectedObjId = newObj->_id;
			}
			ImGui::EndPopup();
		}

		//リネーム用モーダルウィンドウの処理
		if(_openPopup) {
			ImGui::OpenPopup("Rename Object");
			_openPopup = false; //合図をリセット
		}
		ImVec2 center = ImGui::GetMainViewport()->GetCenter();
		ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

		if (ImGui::BeginPopupModal("Rename Object", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
			bool enterPressed = ImGui::InputText("Name", _renameBuffer, sizeof(_renameBuffer), ImGuiInputTextFlags_EnterReturnsTrue);
			//ボタンエリア
			if (ImGui::Button("OK", ImVec2(120, 0)) || enterPressed) {
				//ID空オブジェクトを探して名前を更新
				for (auto& obj : _gameObjects) {
					if (obj->_id == _renameTargetId) {
						obj->_name = GenerateUniqueName(_renameBuffer);
						break;
					}
				}
				ImGui::CloseCurrentPopup();
			}

			ImGui::SameLine();

			if (ImGui::Button("Cancel", ImVec2(120, 0))) {
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}

	}
	ImGui::End();

}

void Scene::RenderInspector()
{
	ImGui::Begin("Inspector");
	{
		if (_selectedObjId != -1) {
			//IDからオブジェクトのポインタ取得
			std::shared_ptr<GameObject> selectedObj = nullptr;
			for (auto& gameObject : _gameObjects) {
				if (gameObject->_id == _selectedObjId) {
					selectedObj = gameObject;
					break;
				}
			}

			if (selectedObj) {
				ImGui::Text("%s", selectedObj->_name.c_str());
				ImGui::Checkbox("Active", &selectedObj->_active);
				//Transform
				if (ImGui::BeginTable("Transform", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
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
						ImGui::InputFloat("", &((&selectedObj->_transform._position.x)[i]));
						ImGui::PopID();
					}

					changed = false;
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::Text("Scale");
					for (int i = 0; i < 3; i++) {
						ImGui::TableSetColumnIndex(i + 1);
						ImGui::PushID(i + 4);
						ImGui::InputFloat("", &((&selectedObj->_transform._scale.x)[i]));
						ImGui::PopID();
					}
					changed = false;
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::Text("Rotation");
					for (int i = 0; i < 3; i++) {
						ImGui::TableSetColumnIndex(i + 1);
						ImGui::PushID(i + 7);
						changed |= ImGui::InputFloat("", &((&selectedObj->_transform._rotation.x)[i]));
						ImGui::PopID();
					}
					if (changed) {
						selectedObj->_transform._quaternion = KTQUATERNION::FromEulerAngles(selectedObj->_transform._rotation.x, selectedObj->_transform._rotation.y, selectedObj->_transform._rotation.z);
						if (selectedObj->GetComponent<RigidBody>()) {
							selectedObj->GetComponent<RigidBody>()->_orientation = selectedObj->_transform._quaternion;
							selectedObj->GetComponent<RigidBody>()->_angularVelocity = KTVECTOR3(0.0f, 0.0f, 0.0f);
						}
					}

					ImGui::EndTable();
				}

				for (auto& component : selectedObj->_components) {
					if (ImGui::TreeNode(component->GetComponentName().c_str())) {
						ImGui::Checkbox("Active", &component->_active);
						component->ShowUI();
						ImGui::TreePop();
					}
				}

				ImGui::Separator();
				ImGui::Spacing();

				ComponentRegistry::RenderAddComponentMenu(selectedObj.get());
			
				ImGui::Spacing();
			}
		}

	}
	ImGui::End();
}

void Scene::RenderButton(){
	ImGui::SetNextWindowSize({ 720,50 });
	ImGui::Begin("Mode", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar);
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


