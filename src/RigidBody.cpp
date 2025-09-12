//=====================================================================================
// RigidBody.cpp
// Author:Kaito Aoki
// Date:2025/09/11
//=====================================================================================

#include "RigidBody.h"
#include "GameObject.h"
#include <imgui.h>

void RigidBody::Update() {
	_invMass = (_mass != 0.0f) ? 1.0f / _mass : 0.0f; // ‹tŽ¿—Ê
	if (_useGravity)
		_velocity.y += _gravity * _gravityScale * 0.00001f;
	else
		_velocity.y = 0.0f;
	_owner->_transform._position += _velocity;

}


void RigidBody::ShowUI() {
	//‘¬“x
	if (ImGui::BeginTable("Transform", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
	{

		ImGui::TableSetupColumn("Property");
		ImGui::TableSetupColumn("X");
		ImGui::TableSetupColumn("Y");
		ImGui::TableSetupColumn("Z");
		ImGui::TableHeadersRow();


		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::Text("Velocity");
		for (int i = 0; i < 3; i++) {
			ImGui::TableSetColumnIndex(i + 1);
			ImGui::PushID(i);
			ImGui::InputFloat("", &((&_velocity.x)[i]));
			ImGui::PopID();
		}
		ImGui::EndTable();
	}
	ImGui::InputFloat("Mass", &_mass, 0.1f, 1.0f, "%.3f");
	ImGui::Checkbox("UseGravity", &_useGravity);
}
