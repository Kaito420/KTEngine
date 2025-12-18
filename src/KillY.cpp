//=====================================================================================
// KillY.h
// Author:Kaito Aoki
// Date:2025/12/18
//=====================================================================================

#include "KillY.h"
#include "GameObject.h"
#include "RigidBody.h"

void KillY::Awake(){
	_rigidBody = _owner->GetComponent<RigidBody>();
}

void KillY::Update()
{
	if (_owner->_transform._position.y < _threshold) {
		_owner->_transform._position = _resetPosition;
		if(_rigidBody)
		{
			_rigidBody->_velocity = KTVECTOR3(0.0f, 0.0f, 0.0f);
			_rigidBody->_angularVelocity = KTVECTOR3(0.0f, 0.0f, 0.0f);
		}

	}
}

void KillY::ShowUI(){
	ImGui::DragFloat("Threshold", &_threshold, 0.1f);
	ImGui::DragFloat3("Reset Position", &_resetPosition.x, 0.1f);
}
