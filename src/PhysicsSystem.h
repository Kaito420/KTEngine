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
	std::vector<CollisionManifold> _manifolds;
public:
	void RegisterCollider(Collider* collider) {
		_colliders.push_back(collider);
	}

	void RemoveCollider(Collider* collider) {
		auto it = std::find(_colliders.begin(), _colliders.end(), collider);
		if (it != _colliders.end()) {
			_colliders.erase(it);
		}
	}

	void Update();

	void SetLocalInertiaTensor();

	void ResolveCollision(CollisionManifold& manifold);

	void ResolveInpulse(CollisionManifold& manifold);

	void ClearManifold() { _manifolds.clear(); }
};

#endif // !_PHYSICSSYSTEM_H_