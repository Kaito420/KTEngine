//=====================================================================================
// RigidBody.cpp
// Author:Kaito Aoki
// Date:2025/09/11
//=====================================================================================

#include "RigidBody.h"
#include "GameObject.h"
#include <imgui.h>
#include <cmath>
#include "Collider.h"
#include "Manager.h"
#include "Scene.h"

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

void RigidBody::Sleep() {
	_sleeping = true;
	_velocity = KTVECTOR3(0.0f, 0.0f, 0.0f);
	_angularVelocity = KTVECTOR3(0.0f, 0.0f, 0.0f);
}

void RigidBody::WakeUp() {
	_sleeping = false;
	_sleepTimer = 0.0f;
}

void RigidBody::CheckSleep(float dt) {
	if (_velocity.MagnitudeSqr() < _sleepEpsilon && _angularVelocity.MagnitudeSqr() < _sleepEpsilon) {
		_sleepTimer += dt;
		if (_sleepTimer >= 0.45f)
			Sleep();
	}
	else {
		_sleepTimer = 0.0f;
		WakeUp();
	}
}

void RigidBody::Integrate(float dt){
	if (!_owner || dt <= 0.0f) return;

	// ワールド空間の慣性テンソルに変換（isKinematic が true でもこの計算は行う）
	_orientation = _owner->GetWorldQuaternion();
	KTMATRIX3 R = _orientation.ToMatrix().ToMatrix3();
	_inertiaTensorWorldInv = R * _inertiaTensorBodyInv * R.Transpose();

	if (_sleeping) {
		_sleepTimer += dt;
		return;
	}

	if (_isKinematic) return;

	if (_invMass <= 0.0f) return;

	if (_useGravity)
		_velocity.y += _gravity * _gravityScale * dt;

	// 加速度の計算 a = F/m
	KTVECTOR3 acceleration = _forceAccum * _invMass;
	_velocity += acceleration * dt;

	// フレームレート・サブステップ数に非依存な減衰（60FPS基準の減衰率を指数変換）
	float stepScale = dt * 60.0f;
	_velocity *= powf(Clamp(_linearDamping, 0.0f, 1.0f), stepScale);

	// 位置の更新
	_owner->SetWorldPosition(_owner->GetWorldPosition() + _velocity * dt);

	// 角加速度の計算 = Inverse * トルク
	KTVECTOR3 angularAcceleration = _inertiaTensorWorldInv * _torqueAccum;
	_angularVelocity += angularAcceleration * dt;

	// 角速度の減衰
	_angularVelocity *= powf(Clamp(_angularDamping, 0.0f, 1.0f), stepScale);

	// 姿勢の更新
	KTQUATERNION omegaQuat(_angularVelocity.x, _angularVelocity.y, _angularVelocity.z, 0.0f);
	KTQUATERNION dq = (omegaQuat * _orientation) * 0.5f;
	_orientation = (_orientation + dq * dt).Normalize();

	// 姿勢の反映
	_owner->SetWorldQuaternion(_orientation);
}

void RigidBody::Awake(){

	if (_isKinematic) {
		_invMass = 0.0f;
	}
	else {
		_invMass = (_mass != 0.0f) ? 1.0f / _mass : 0.0f;
	}

	_inertiaTensorBody = KTMATRIX3::Identity();
	_inertiaTensorBodyInv = KTMATRIX3::Identity();

}

void RigidBody::Start(){
	if (Manager::GetCurrentScene() && Manager::GetCurrentScene()->GetPhysicsSystem()) {
		Manager::GetCurrentScene()->GetPhysicsSystem()->RegisterRigidBody(this);
	}
}

void RigidBody::Update() {
	if (Manager::GetCurrentScene() && Manager::GetCurrentScene()->GetPhysicsSystem()) {
		Manager::GetCurrentScene()->GetPhysicsSystem()->RegisterRigidBody(this);
	}
	if (_isKinematic) {
		_invMass = 0.0f;
		_velocity = KTVECTOR3(0.0f, 0.0f, 0.0f);
		_angularVelocity = KTVECTOR3(0.0f, 0.0f, 0.0f);
		_forceAccum = KTVECTOR3(0.0f, 0.0f, 0.0f);
		_torqueAccum = KTVECTOR3(0.0f, 0.0f, 0.0f);
	}
	else {
		_invMass = (_mass != 0.0f) ? 1.0f / _mass : 0.0f;
	}

}

void RigidBody::OnDestroy(){
	if (Manager::GetCurrentScene() && Manager::GetCurrentScene()->GetPhysicsSystem()) {
		Manager::GetCurrentScene()->GetPhysicsSystem()->RemoveRigidBody(this);
	}
}


void RigidBody::ShowUI() {
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

		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::Text("AngularVel");
		for (int i = 0; i < 3; i++) {
			ImGui::TableSetColumnIndex(i + 1);
			ImGui::PushID(i + 3);
			ImGui::InputFloat("", &((&_angularVelocity.x)[i]));
			ImGui::PopID();
		}

		ImGui::EndTable();
	}
	ImGui::InputFloat("Mass", &_mass);
	ImGui::SliderFloat("Restitution", &_restitution, 0.0f, 1.0f);
	ImGui::SliderFloat("StaticFriction", &_staticFriction, 0.0f, 1.0f);
	ImGui::SliderFloat("DynamicFriction", &_dynamicFriction, 0.0f, 1.0f);
	ImGui::SliderFloat("LinearDamping", &_linearDamping, 0.0f, 1.0f);
	ImGui::SliderFloat("AngularDamping", &_angularDamping, 0.0f, 1.0f);
	ImGui::Checkbox("UseGravity", &_useGravity);
	ImGui::Checkbox("IsKinematic", &_isKinematic);
	ImGui::Checkbox("Sleeping", &_sleeping);
}