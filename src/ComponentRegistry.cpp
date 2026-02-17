//=====================================================================================
// ComponentRegistry.cpp
// Author:Kaito Aoki
// Date:2026/02/17
//=====================================================================================

#include "ComponentRegistry.h"
#include "imgui.h"
#include <algorithm>

//静的メンバの定義
std::map<std::string, ComponentRegistry::CreatorFunc> ComponentRegistry::_creators;

void ComponentRegistry::RenderAddComponentMenu(GameObject* target){

	//ボタンが押されたらポップアップを開く
	if (ImGui::Button("Add Component", ImVec2(-1, 0))) {
		ImGui::OpenPopup("AddComponentPopup");
	}

	if (ImGui::BeginPopup("AddComponentPopup")) {
		//簡易的な検索機能
		static char filterBuffer[128] = "";
		ImGui::InputTextWithHint("##Filter", "Search...", filterBuffer, sizeof(filterBuffer));

		ImGui::Separator();

		for (auto& pair : _creators) {
			const std::string& name = pair.first;

			if (strlen(filterBuffer) > 0) {
				if (name.find(filterBuffer) == std::string::npos) {
					continue;
				}
			}

			//メニューアイテムを表示
			if (ImGui::Selectable(name.c_str())) {
				pair.second(target);//選択されたらAddComponent実行
				ImGui::CloseCurrentPopup();
			}
		}
		ImGui::EndPopup();
	}
}
