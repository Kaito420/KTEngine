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

public:
	bool _useGravity = false;
	float _mass = 1.0f;
	float _invMass = (_mass != 0.0f) ? 1.0f / _mass : 0.0f; // ãtéøó 
	float _restitution = 1.0f; // îΩî≠åWêî

	KTVECTOR3 _velocity = KTVECTOR3(0.0f, 0.0f, 0.0f);

	void Update() override;

	std::string GetComponentName()override { return "RigidBody"; }

	void ShowUI() override;
};

#endif // !_RIGIDBODY_H_