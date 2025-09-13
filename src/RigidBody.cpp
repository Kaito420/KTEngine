//=====================================================================================
// RigidBody.cpp
// Author:Kaito Aoki
// Date:2025/09/11
//=====================================================================================

#include "RigidBody.h"
#include "GameObject.h"
#include <imgui.h>

KTMATRIX3 RigidBody::InertiaTensorSphere(float mass, float radius){
	float v = (2.0f / 5.0f) * mass * radius * radius;
	return KTMATRIX3(
		v, 0.0f, 0.0f,
		0.0f, v, 0.0f,
		0.0f, 0.0f, v
	);

}

KTMATRIX3 RigidBody::InertiaTensorBox(float mass, const KTVECTOR3& halfSize){
	float ix = (1.0f / 12.0f) * mass * (halfSize.y * halfSize.y + halfSize.z * halfSize.z) * 4.0f;
	float iy = (1.0f / 12.0f) * mass * (halfSize.x * halfSize.x + halfSize.z * halfSize.z) * 4.0f;
	float iz = (1.0f / 12.0f) * mass * (halfSize.x * halfSize.x + halfSize.y * halfSize.y) * 4.0f;
	return KTMATRIX3(
		ix, 0.0f, 0.0f,
		0.0f, iy, 0.0f,
		0.0f, 0.0f, iz
	);
}

void RigidBody::IntegrateRotation(){
	//ワールド空間の慣性テンソルに変換
	KTMATRIX3 R = _orientation.ToMatrix().ToMatrix3();//回転行列
	_inertiaTensorWorldInv = R * _inertiaTensorBodyInv * R.Transposed();

	//角加速度の計算 = Inverse * トルク
	KTVECTOR3 angularAcceleration = _inertiaTensorWorldInv * _torqueAccum;

	_angularVelocity += angularAcceleration * DT;

	//姿勢の更新
	KTQUATERNION deltaOrientation(
		_angularVelocity.x * 0.5f * DT,
		_angularVelocity.y * 0.5f * DT,
		_angularVelocity.z * 0.5f * DT,
		0.0f
	);
	_orientation = (_orientation + deltaOrientation * _orientation).Normalize();

	//トルクのリセット
	_torqueAccum = KTVECTOR3(0.0f, 0.0f, 0.0f);

}

void RigidBody::Update() {
	_invMass = (_mass != 0.0f) ? 1.0f / _mass : 0.0f; // 逆質量
	if (_useGravity)
		_velocity.y += _gravity * _gravityScale * DT;
	else
		_velocity.y = 0.0f;
	_owner->_transform._position += _velocity;

}


void RigidBody::ShowUI() {
	//速度
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
