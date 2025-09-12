//=====================================================================================
// PhysicsSystem.h
// Author:Kaito Aoki
// Date:2025/09/09
//=====================================================================================

#ifndef _PHYSICSSYSTEM_H_
#define _PHYSICSSYSTEM_H_

#include <vector>
#include "GameObject.h"
#include "Collider.h"
#include "RigidBody.h"

class PhysicsSystem {
	std::vector<Collider*> _colliders;
public:
	void RegisterCollider(Collider* collider) {
		_colliders.push_back(collider);
	}

	void Update();
};

#endif // !_PHYSICSSYSTEM_H_