//=====================================================================================
// RigidBody.h
// Author:Kaito Aoki
// Date:2025/09/11
//=====================================================================================

#ifndef _RIGIDBODY_H_
#define _RIGIDBODY_H_

#include "Component.h"
#include "ktvector.hpp"
#include <cereal/types/base_class.hpp>
#include <cereal/types/polymorphic.hpp>

class RigidBody : public Component {
	friend class cereal::access;
private:
	float _gravity = -9.8f;
	float _gravityScale = 1.0f;

	bool _sleeping = false;
	float _sleepTimer = 0.0f;
	float _sleepEpsilon = 0.001f;

	KTVECTOR3 _forceAccum = KTVECTOR3(0.0f, 0.0f, 0.0f);

public:
	bool _useGravity = true;
	bool _isKinematic = false;
	float _mass = 1.0f;
	float _oldMass;
	float _invMass = (_mass != 0.0f) ? 1.0f / _mass : 0.0f;
	float _restitution = 0.0f;

	float _staticFriction = 0.6f;
	float _dynamicFriction = 0.4f;

	KTQUATERNION _orientation;
	KTVECTOR3 _angularVelocity;
	KTVECTOR3 _torqueAccum;
	KTMATRIX3 _inertiaTensorBody;
	KTMATRIX3 _inertiaTensorBodyInv;
	KTMATRIX3 _inertiaTensorWorld;
	KTMATRIX3 _inertiaTensorWorldInv;

	float _linearDamping = 0.99f;
	float _angularDamping = 0.98f;

	KTVECTOR3 _velocity = KTVECTOR3(0.0f, 0.0f, 0.0f);
	KTVECTOR3 _positionCorrection = KTVECTOR3(0.0f, 0.0f, 0.0f);

	KTMATRIX3 InertiaTensorSphere(float mass, float radius);

	KTMATRIX3 InertiaTensorBox(float mass, const KTVECTOR3& halfSize);

	void ApplyForce(const KTVECTOR3& force) {
		_forceAccum += force;
	}

	void ApplyTorque(const KTVECTOR3& torque) {
		_torqueAccum += torque;
	}

	void ClearForces() {
		_forceAccum = KTVECTOR3(0.0f, 0.0f, 0.0f);
		_torqueAccum = KTVECTOR3(0.0f, 0.0f, 0.0f);
	}

	void Sleep();

	void WakeUp();

	void CheckSleep(float dt);

	bool IsSleeping() const {
		return _sleeping;
	}

	void Integrate(float dt);

	void Awake() override;
	void Start() override;
	void Update() override;
	void OnDestroy()override;

	std::string GetComponentName()override { return "RigidBody"; }

	void ShowUI() override;

	template <class Archive>
	void serialize(Archive& ar) {
		ar(cereal::base_class<Component>(this));
		ar(cereal::make_nvp("Mass", _mass));
		ar(cereal::make_nvp("UseGravity", _useGravity));
		ar(cereal::make_nvp("GravityScale", _gravityScale));
		ar(cereal::make_nvp("IsKinematic", _isKinematic));
		ar(cereal::make_nvp("Restitution", _restitution));
		ar(cereal::make_nvp("StaticFriction", _staticFriction));
		ar(cereal::make_nvp("DynamicFriction", _dynamicFriction));
		ar(cereal::make_nvp("LinearDamping", _linearDamping));
		ar(cereal::make_nvp("AngularDamping", _angularDamping));
		ar(cereal::make_nvp("Velocity", _velocity));
		ar(cereal::make_nvp("AngularVelocity", _angularVelocity));
	}
};

#endif // !_RIGIDBODY_H_