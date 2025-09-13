//=====================================================================================
// RigidBody.h
// Author:Kaito Aoki
// Date:2025/09/11
//=====================================================================================

#ifndef _RIGIDBODY_H_
#define _RIGIDBODY_H_

#include "Component.h"
#include "ktvector.hpp"

class RigidBody : public Component {
private:
	float _gravity = -9.8f;
	float _gravityScale = 1.0f;

	KTQUATERNION _orientation;	//姿勢
	KTVECTOR3 _angularVelocity; //角速度
	KTVECTOR3 _torqueAccum; //トルクの蓄積
	KTMATRIX3 _inertiaTensorBody; // 慣性テンソル
	KTMATRIX3 _inertiaTensorBodyInv; // 逆慣性テンソル
	KTMATRIX3 _inertiaTensorWorld; // ワールド空間での慣性テンソル
	KTMATRIX3 _inertiaTensorWorldInv; // ワールド空間での逆慣性テンソル

public:
	bool _useGravity = false;
	float _mass = 1.0f;
	float _invMass = (_mass != 0.0f) ? 1.0f / _mass : 0.0f; // 逆質量
	float _restitution = 0.0f; // 反発係数

	float _staticFriction = 0.6f;  // 静止摩擦係数
	float _dynamicFriction = 0.4f; // 動摩擦係数

	KTVECTOR3 _velocity = KTVECTOR3(0.0f, 0.0f, 0.0f);

	KTMATRIX3 InertiaTensorSphere(float mass, float radius);

	/// <summary>
	/// 
	/// </summary>
	/// <param name="mass"></param>
	/// <param name="halfSize">col->extentsそのままでOK</param>
	/// <returns></returns>
	KTMATRIX3 InertiaTensorBox(float mass, const KTVECTOR3& halfSize);

	void ApplyTorque(const KTVECTOR3& torque) {
		_torqueAccum += torque;
	}

	void IntegrateRotation();

	void Update() override;

	std::string GetComponentName()override { return "RigidBody"; }

	void ShowUI() override;
};

#endif // !_RIGIDBODY_H_