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
#include "CapsuleObject.h"
#include "Profiler.h"
#include "Collider.h"

std::string Scene::GenerateUniqueName(const std::string& baseName){
	std::string uniqueName = baseName;
	int index = 1;

	// 重複する名前を検出してインデックスを付与
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
	_physicsAccumulator = 0.0f;

	AddGameObject<Camera>()->_name = "Camera";
	AddGameObject<Sky>()->_name = "Sky";
	AddGameObject<Light>()->_name = "Light";
}

void Scene::Finalize() {
	for (auto& gameObject : _gameObjects) {
		gameObject->OnDestroy();
		gameObject->_parent.reset();
		gameObject->_children.clear();
	}
}

void Scene::Update(){
	int activeCount = 0;
	{
		PROFILE_SCOPE("Game Logic");
		for( auto& gameObject : _gameObjects ){
			if (gameObject->GetActive()) activeCount++;
			if(gameObject->GetActive() && gameObject->GetStarted() == false) {
				gameObject->Start();
				gameObject->Started();
			}
			if (gameObject->GetActive()) {
				gameObject->Update();
				gameObject->UpdateComponents();
			}
		}
	}
	Profiler::RecordSceneObjects((int)_gameObjects.size(), activeCount);

	if (_physicsSystem) {
		PROFILE_SCOPE("Physics");
		float frameDt = Profiler::GetLastFrameTime() * 0.001f;
		if (frameDt <= 0.0f) frameDt = 1.0f / 60.0f;
		if (frameDt > 0.1f) frameDt = 0.1f;

		_physicsAccumulator += frameDt;
		float fixedDt = _physicsSystem->GetFixedDeltaTime();

		while (_physicsAccumulator >= fixedDt) {
			for (auto& gameObject : _gameObjects) {
				if (gameObject->GetActive()) {
					gameObject->FixedUpdate();
					gameObject->FixedUpdateComponents();
				}
			}
			_physicsSystem->Step(fixedDt);
			_physicsAccumulator -= fixedDt;
		}
	}

	{
		PROFILE_SCOPE("Late Update");
		for( auto& gameObject : _gameObjects ){
			if (gameObject->GetActive()) {
				gameObject->LateUpdate();
				gameObject->LateUpdateComponents();
			}
		}
	}

			// 削除
	_gameObjects.remove_if([](const std::shared_ptr<GameObject>& obj) { 
		if (obj->IsDestroy())
			obj->ProcessDestroyComponents();
		return obj->IsDestroy();
	});
}

void Scene::UpdateEditor() {
	PROFILE_SCOPE("Editor Logic");
	int activeCount = 0;
	for (auto& gameObject : _gameObjects) {
		if (gameObject->GetActive()) {
			activeCount++;
			gameObject->UpdateEditor();
		}
	}
	Profiler::RecordSceneObjects((int)_gameObjects.size(), activeCount);

	if (_physicsSystem) {
		PROFILE_SCOPE("Physics");
		_physicsSystem->UpdateEditor();
	}

			// クリア
	_gameObjects.remove_if([](const std::shared_ptr<GameObject>& obj) {
		if (obj->IsDestroy())
			obj->ProcessDestroyComponents();
		return obj->IsDestroy();
		});
}

void Scene::Render()const{
	for(const auto& gameObject : _gameObjects){
		if (gameObject->GetActive()) {
			gameObject->Render();
			gameObject->RenderComponents();
		}
	}
}

void Scene::OnLoaded() {
	if(_physicsSystem)
		delete _physicsSystem;
	_physicsSystem = new PhysicsSystem();
	_physicsAccumulator = 0.0f;
	uint32_t maxId = 0;

	// 親子関係のキャッシュを一旦クリア
	for (auto& gameObject : _gameObjects) {
		gameObject->_parent.reset();
		gameObject->_children.clear();
	}

	for(auto& gameObject: _gameObjects) {
		gameObject->OnLoaded();
		if(maxId < gameObject->_id)
			maxId = gameObject->_id;

		for (auto& comp : gameObject->_components) {
			if (auto* col = dynamic_cast<Collider*>(comp.get())) {
				_physicsSystem->RegisterCollider(col);
			}
		}
	}

	// _parentId から親子リンク（_parent と _children）を復元
	for (auto& gameObject : _gameObjects) {
		if (gameObject->_parentId != -1) {
			for (auto& candidate : _gameObjects) {
				if ((int)candidate->_id == gameObject->_parentId && candidate.get() != gameObject.get()) {
					gameObject->_parent = candidate;
					candidate->_children.push_back(gameObject);
					break;
				}
			}
			if (!gameObject->_parent) {
				gameObject->_parentId = -1;
			}
		}
	}

	GameObject::_nextId = maxId + 1;
}

namespace {
	struct PendingHierarchyDrop {
		GameObject* src    = nullptr;
		GameObject* target = nullptr;
		// 0: MakeChild, 1: InsertBefore, 2: InsertAfter, 3: MoveToRootTop, 4: MoveToRootBottom
		int mode           = -1;
	};
	static PendingHierarchyDrop s_pendingHierarchyDrop;
}

void Scene::RenderHierarchyNode(const std::shared_ptr<GameObject>& gameObject) {
	if (!gameObject || gameObject->IsDestroy()) return;

	ImGui::PushID(gameObject->_id);

	bool hasChildren = !gameObject->GetChildren().empty();
	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DefaultOpen;
	if (_selectedObjId == (int)gameObject->_id) {
		flags |= ImGuiTreeNodeFlags_Selected;
	}
	if (!hasChildren) {
		flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
	}

	// 非アクティブ（自身または親が非アクティブ）の場合は文字色を半透明にする
	bool isEffectiveActive = gameObject->GetActive();
	if (!isEffectiveActive) {
		ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
	}

	bool nodeOpen = ImGui::TreeNodeEx(gameObject->_name.c_str(), flags);

	if (!isEffectiveActive) {
		ImGui::PopStyleColor();
	}

	ImVec2 itemMin = ImGui::GetItemRectMin();
	ImVec2 itemMax = ImGui::GetItemRectMax();
	float itemHeight = itemMax.y - itemMin.y;

	if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
		_selectedObjId = gameObject->_id;
	}

	// Drag Source
	if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
		GameObject* srcPtr = gameObject.get();
		ImGui::SetDragDropPayload("DND_GAMEOBJECT", &srcPtr, sizeof(GameObject*));
		ImGui::Text("%s", gameObject->_name.c_str());
		ImGui::EndDragDropSource();
	}

	// Drag & Drop Target (上端28%: 直前に挿入 / 中央44%: 親子化 / 下端28%: 直後に挿入)
	if (ImGui::BeginDragDropTarget()) {
		float mouseY = ImGui::GetIO().MousePos.y;
		float relY = (itemHeight > 1.0f) ? ((mouseY - itemMin.y) / itemHeight) : 0.5f;
		int dropMode = 0; // 0: MakeChild, 1: InsertBefore, 2: InsertAfter
		if (relY < 0.28f) {
			dropMode = 1;
		} else if (relY > 0.72f) {
			dropMode = 2;
		}

		const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(
			"DND_GAMEOBJECT",
			ImGuiDragDropFlags_AcceptBeforeDelivery | ImGuiDragDropFlags_AcceptNoDrawDefaultRect
		);
		if (payload && payload->Data) {
			GameObject* srcPtr = *(GameObject**)payload->Data;
			bool validDrop = (srcPtr != nullptr && srcPtr != gameObject.get());
			if (validDrop) {
				if (dropMode == 0) {
					validDrop = !srcPtr->IsAncestorOf(gameObject);
				} else {
					auto targetParent = gameObject->GetParent();
					if (targetParent && (srcPtr == targetParent.get() || srcPtr->IsAncestorOf(targetParent))) {
						validDrop = false;
					}
				}
			}

			if (validDrop) {
				ImDrawList* drawList = ImGui::GetWindowDrawList();
				ImU32 lineColor = IM_COL32(66, 175, 255, 255);
				ImU32 boxColor  = IM_COL32(66, 175, 255, 210);
				ImU32 fillColor = IM_COL32(66, 175, 255, 45);

				if (dropMode == 1) {
					float y = itemMin.y;
					drawList->AddLine(ImVec2(itemMin.x + 4.0f, y), ImVec2(itemMax.x, y), lineColor, 2.5f);
					drawList->AddCircleFilled(ImVec2(itemMin.x + 6.0f, y), 3.5f, lineColor);
				} else if (dropMode == 2) {
					float y = itemMax.y;
					drawList->AddLine(ImVec2(itemMin.x + 4.0f, y), ImVec2(itemMax.x, y), lineColor, 2.5f);
					drawList->AddCircleFilled(ImVec2(itemMin.x + 6.0f, y), 3.5f, lineColor);
				} else {
					drawList->AddRectFilled(itemMin, itemMax, fillColor, 3.0f);
					drawList->AddRect(itemMin, itemMax, boxColor, 3.0f, 0, 1.5f);
				}

				if (payload->IsDelivery()) {
					s_pendingHierarchyDrop.src    = srcPtr;
					s_pendingHierarchyDrop.target = gameObject.get();
					s_pendingHierarchyDrop.mode   = dropMode;
				}
			}
		}
		ImGui::EndDragDropTarget();
	}

	// 右クリックでコンテキストメニュー
	if (ImGui::BeginPopupContextItem()) {
		if (gameObject->GetParent() != nullptr) {
			if (ImGui::MenuItem("Unparent")) {
				gameObject->SetParent(nullptr, true);
			}
			ImGui::Separator();
		}
		if (ImGui::MenuItem("Create Child Empty")) {
			GameObject* newObj = AddGameObject<GameObject>();
			newObj->_name = GenerateUniqueName("GameObject");
			newObj->SetParent(gameObject, false);
			_selectedObjId = newObj->_id;
		}
		ImGui::Separator();
		if (ImGui::MenuItem("Rename")) {
			_renameTargetId = gameObject->_id;
			strcpy_s(_renameBuffer, sizeof(_renameBuffer), gameObject->_name.c_str());
			_openPopup = true;
		}
		if (ImGui::MenuItem("Delete")) {
			gameObject->Destroy();
		}
		ImGui::EndPopup();
	}

	// 子ノードの再帰描画
	if (hasChildren && nodeOpen) {
		// ループ中に親子変更が起きても安全なようにコピーで走査
		std::vector<std::shared_ptr<GameObject>> childrenCopy = gameObject->GetChildren();
		for (const auto& child : childrenCopy) {
			RenderHierarchyNode(child);
		}
		ImGui::TreePop();
	}

	ImGui::PopID();
}

void Scene::RenderHierarchy()
{
	ImGui::Begin("Hierarchy View"); 
	{
		auto drawCreateObjectMenu = [this]() {
			if (ImGui::MenuItem("Create Empty")) {
				GameObject* newObj = AddGameObject<GameObject>();
				newObj->_name = GenerateUniqueName("GameObject");
				_selectedObjId = newObj->_id;
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
			if (ImGui::MenuItem("Capsule")) {
				GameObject* newObj = AddGameObject<CapsuleObject>();
				newObj->_name = GenerateUniqueName("Capsule");
				_selectedObjId = newObj->_id;
			}
		};

		auto acceptRootUnparentDrop = [](int rootDropMode) {
			if (ImGui::BeginDragDropTarget()) {
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DND_GAMEOBJECT")) {
					GameObject* srcPtr = *(GameObject**)payload->Data;
					if (srcPtr) {
						s_pendingHierarchyDrop.src    = srcPtr;
						s_pendingHierarchyDrop.target = nullptr;
						s_pendingHierarchyDrop.mode   = rootDropMode;
					}
				}
				ImGui::EndDragDropTarget();
			}
		};

		s_pendingHierarchyDrop = {};

		// 上部固定ヘッダー領域：「+」ボタンと上部ドロップ＆右クリック領域
		if (ImGui::Button("+")) {
			ImGui::OpenPopup("HierarchyCreateObjectPopup");
		}
		if (ImGui::BeginPopup("HierarchyCreateObjectPopup")) {
			drawCreateObjectMenu();
			ImGui::EndPopup();
		}

		ImGui::SameLine();
		ImVec2 headerAvail = ImGui::GetContentRegionAvail();
		float headerWidth = headerAvail.x > 1.0f ? headerAvail.x : 1.0f;
		ImGui::InvisibleButton("##HierarchyHeaderDropZone", ImVec2(headerWidth, ImGui::GetFrameHeight()));
		acceptRootUnparentDrop(3); // 3: MoveToRootTop
		if (ImGui::BeginPopupContextItem("HierarchyHeaderContextPopup")) {
			drawCreateObjectMenu();
			ImGui::EndPopup();
		}

		ImGui::Separator();

		// 下部スクロール領域（+ボタンを上部に固定したままツリーのみをスクロールさせる）
		if (ImGui::BeginChild("HierarchyScrollRegion", ImVec2(0, 0), false)) {
			// ルートオブジェクト（親を持たないオブジェクト）のみをトップレベルに描画
			for (auto it = _gameObjects.begin(); it != _gameObjects.end(); ++it) {
				auto& gameObject = *it;
				if (gameObject->GetParent() == nullptr) {
					RenderHierarchyNode(gameObject);
				}
			}

			// 下部の余白エリア（子オブジェクトをD&Dすると親子関係を解除してルート末尾へ移動）
			ImVec2 avail = ImGui::GetContentRegionAvail();
			if (avail.x < 1.0f) avail.x = 1.0f;
			if (avail.y < 40.0f) avail.y = 40.0f;
			ImGui::InvisibleButton("##HierarchyRootDropZone", avail);
			acceptRootUnparentDrop(4); // 4: MoveToRootBottom

			// 余白エリア（InvisibleButton）上での右クリックメニュー
			if (ImGui::BeginPopupContextItem("HierarchyEmptyItemContextPopup")) {
				drawCreateObjectMenu();
				ImGui::EndPopup();
			}

			// その他の何もない場所での右クリックメニュー
			if (ImGui::BeginPopupContextWindow("HierarchyEmptyWindowContextPopup",
				ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
				drawCreateObjectMenu();
				ImGui::EndPopup();
			}
		}
		ImGui::EndChild();

		// D&D による親子化・並び替えの確定処理（イテレータ安全のためループ外で実行）
		if (s_pendingHierarchyDrop.mode >= 0 && s_pendingHierarchyDrop.src != nullptr) {
			auto srcIt = std::find_if(_gameObjects.begin(), _gameObjects.end(),
				[](const std::shared_ptr<GameObject>& o) { return o.get() == s_pendingHierarchyDrop.src; });
			auto targetIt = std::find_if(_gameObjects.begin(), _gameObjects.end(),
				[](const std::shared_ptr<GameObject>& o) { return o.get() == s_pendingHierarchyDrop.target; });

			if (srcIt != _gameObjects.end()) {
				std::shared_ptr<GameObject> srcObj = *srcIt;
				if (s_pendingHierarchyDrop.mode == 3) {
					// ヘッダー領域へのドロップ：親解除＋ルート先頭へ移動
					if (srcObj->GetParent() != nullptr) {
						srcObj->SetParent(nullptr, true);
					}
					_gameObjects.splice(_gameObjects.begin(), _gameObjects, srcIt);
				}
				else if (s_pendingHierarchyDrop.mode == 4) {
					// 下部余白へのドロップ：親解除＋ルート末尾へ移動
					if (srcObj->GetParent() != nullptr) {
						srcObj->SetParent(nullptr, true);
					}
					_gameObjects.splice(_gameObjects.end(), _gameObjects, srcIt);
				}
				else if (targetIt != _gameObjects.end() && srcIt != targetIt) {
					std::shared_ptr<GameObject> targetObj = *targetIt;
					if (s_pendingHierarchyDrop.mode == 0) {
						// 中央エリアへのドロップ：targetObj の子オブジェクトにする
						if (!srcObj->IsAncestorOf(targetObj)) {
							srcObj->SetParent(targetObj, true);
						}
					}
					else if (s_pendingHierarchyDrop.mode == 1 || s_pendingHierarchyDrop.mode == 2) {
						// 上端(1:直前) / 下端(2:直後) エリアへのドロップ：targetObj と同じ親階層で並び替え
						bool insertAfter = (s_pendingHierarchyDrop.mode == 2);
						std::shared_ptr<GameObject> desiredParent = targetObj->GetParent();
						if (!desiredParent || (srcObj != desiredParent && !srcObj->IsAncestorOf(desiredParent))) {
							if (srcObj->GetParent() != desiredParent) {
								srcObj->SetParent(desiredParent, true);
							}

							// 親がいる場合は親の _children ベクタ内の順序も更新
							if (desiredParent) {
								auto& siblings = desiredParent->_children;
								siblings.erase(
									std::remove_if(siblings.begin(), siblings.end(),
										[&](const std::shared_ptr<GameObject>& c) { return c.get() == srcObj.get(); }),
									siblings.end()
								);
								auto sibTargetIt = std::find_if(siblings.begin(), siblings.end(),
									[&](const std::shared_ptr<GameObject>& c) { return c.get() == targetObj.get(); });
								if (sibTargetIt != siblings.end()) {
									if (insertAfter) ++sibTargetIt;
									siblings.insert(sibTargetIt, srcObj);
								} else {
									siblings.push_back(srcObj);
								}
							}

							// Scene::_gameObjects リスト内の順序も更新（ルート表示順＆シリアライズ順の同期）
							auto insertPos = targetIt;
							if (insertAfter) {
								++insertPos;
							}
							if (insertPos != srcIt) {
								_gameObjects.splice(insertPos, _gameObjects, srcIt);
							}
						}
					}
				}
			}
			s_pendingHierarchyDrop = {};
		}

		// リネーム用モーダルウィンドウの処理
		if(_openPopup) {
			ImGui::OpenPopup("Rename Object");
			_openPopup = false;
		}
		ImVec2 center = ImGui::GetMainViewport()->GetCenter();
		ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

		if (ImGui::BeginPopupModal("Rename Object", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
			bool enterPressed = ImGui::InputText("Name", _renameBuffer, sizeof(_renameBuffer), ImGuiInputTextFlags_EnterReturnsTrue);
			if (ImGui::Button("OK", ImVec2(120, 0)) || enterPressed) {
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
	// IDからGameObjectのポインタ取得
			std::shared_ptr<GameObject> selectedObj = nullptr;
			for (auto& gameObject : _gameObjects) {
				if (gameObject->_id == _selectedObjId) {
					selectedObj = gameObject;
					break;
				}
			}

			if (selectedObj) {
				ImGui::Text("%s", selectedObj->_name.c_str());
				if (selectedObj->GetParent() != nullptr) {
					ImGui::SameLine();
					ImGui::TextDisabled("(Parent: %s)", selectedObj->GetParent()->_name.c_str());
					ImGui::SameLine();
					if (ImGui::SmallButton("Unparent")) {
						selectedObj->SetParent(nullptr, true);
					}
				}
				ImGui::Checkbox("Active", &selectedObj->_active);
				if (selectedObj->GetParent() != nullptr && !selectedObj->GetParent()->GetActive()) {
					ImGui::SameLine();
					ImGui::TextDisabled("(Inactive in hierarchy)");
				}
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

				selectedObj->ShowUI();

				ImGui::Separator();

				Component* componentToRemove = nullptr;

				for (auto& component : selectedObj->_components) {
						ImGui::PushID(component.get()); // ID衝突防止

					bool open = ImGui::TreeNode(component->GetComponentName().c_str());

					if (ImGui::BeginPopupContextItem()) {
						if (ImGui::MenuItem("Remove Component")) {
						componentToRemove = component.get(); // 削除対象
						}
						ImGui::EndPopup();
					}

					if (open) {
						ImGui::Checkbox("Active", &component->_active);
						component->ShowUI();
						ImGui::TreePop();
					}
					ImGui::PopID();
				}

				// ループ後に削除
				if (componentToRemove) {
					selectedObj->RemoveComponent(componentToRemove);
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
	// Manager::RenderToolbar() に移行済み
}




std::shared_ptr<GameObject> Scene::GetSelectedGameObject() {
	if (_selectedObjId != -1) {
		for (auto& gameObject : _gameObjects) {
			if (gameObject->_id == _selectedObjId) {
				return gameObject;
			}
		}
	}
	return nullptr;
}
