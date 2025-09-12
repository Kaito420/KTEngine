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
		_velocity.y += _gravity * 0.00001f;
	else
		_velocity.y = 0.0f;

	_owner->_transform._position += _velocity;

}


void RigidBody::ShowUI() {
	//‘¬“x
	ImGui::Text("vel.x: %.3f vel.y: %.3f vel.z: %.3f", _velocity.x, _velocity.y, _velocity.z);
	ImGui::InputFloat("Mass", &_mass, 0.1f, 1.0f, "%.3f");
	ImGui::Checkbox("UseGravity", &_useGravity);
}
