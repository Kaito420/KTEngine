//=====================================================================================
// RigidBody.cpp
// Author:Kaito Aoki
// Date:2025/09/11
//=====================================================================================

#include "RigidBody.h"
#include "GameObject.h"
#include <imgui.h>

void RigidBody::Update() {
	if (_useGravity)
		_velocity.y += _gravity * _mass * 0.00001f;
	else
		_velocity.y = 0.0f;
	_owner->_transform._position += _velocity;

}


void RigidBody::ShowUI() {
	ImGui::Checkbox("UseGravity", &_useGravity);
}
