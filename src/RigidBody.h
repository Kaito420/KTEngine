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

	KTVECTOR3 _forceAccum = KTVECTOR3(0.0f, 0.0f, 0.0f); // —Í‚Ì’~Ï

public:
	bool _useGravity = false;
	float _mass = 1.0f;
	float _invMass = (_mass != 0.0f) ? 1.0f / _mass : 0.0f; // ‹t¿—Ê
	float _restitution = 0.0f; // ”½”­ŒW”

	float _staticFriction = 0.6f;  // Ã~–€CŒW”
	float _dynamicFriction = 0.4f; // “®–€CŒW”

	KTQUATERNION _orientation;	//p¨
	KTVECTOR3 _angularVelocity; //Šp‘¬“x
	KTVECTOR3 _torqueAccum; //ƒgƒ‹ƒN‚Ì’~Ï
	KTMATRIX3 _inertiaTensorBody; // Šµ«ƒeƒ“ƒ\ƒ‹
	KTMATRIX3 _inertiaTensorBodyInv; // ‹tŠµ«ƒeƒ“ƒ\ƒ‹
	KTMATRIX3 _inertiaTensorWorld; // ƒ[ƒ‹ƒh‹óŠÔ‚Å‚ÌŠµ«ƒeƒ“ƒ\ƒ‹
	KTMATRIX3 _inertiaTensorWorldInv; // ƒ[ƒ‹ƒh‹óŠÔ‚Å‚Ì‹tŠµ«ƒeƒ“ƒ\ƒ‹

	float _linearDamping = 0.99f; // üŒ`Œ¸Š
	float _angularDamping = 0.98f; // ŠpŒ¸Š

	KTVECTOR3 _velocity = KTVECTOR3(0.0f, 0.0f, 0.0f);

	KTMATRIX3 InertiaTensorSphere(float mass, float radius);

	/// <summary>
	/// 
	/// </summary>
	/// <param name="mass"></param>
	/// <param name="halfSize">col->extents‚»‚Ì‚Ü‚Ü‚ÅOK</param>
	/// <returns></returns>
	KTMATRIX3 InertiaTensorBox(float mass, const KTVECTOR3& halfSize);

	void ApplyTorque(const KTVECTOR3& torque) {
		_torqueAccum += torque;
	}

	void Integrate();

	void Awake() override;

	void Update() override;

	std::string GetComponentName()override { return "RigidBody"; }

	void ShowUI() override;
};

#endif // !_RIGIDBODY_H_