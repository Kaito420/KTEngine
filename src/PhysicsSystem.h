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

struct ManifoldKey {
	uint64_t key;
	CollisionManifold* manifold;

	bool operator<(const ManifoldKey& other)const {
		return key < other.key;
	}
};

class PhysicsSystem {
	std::vector<Collider*> _colliders;
	std::vector<RigidBody*> _rigidBodys;
	std::vector<CollisionManifold> _manifolds;
	std::vector<CollisionManifold> _prevManifolds;
	uint64_t MakePairKey(Collider* a, Collider* b);
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

	void RegisterRigidBody(RigidBody* rigidBody) {
		_rigidBodys.push_back(rigidBody);
	}

	void RemoveRigidBody(RigidBody* rigidBody) {
		auto it = std::find(_rigidBodys.begin(), _rigidBodys.end(), rigidBody);
		if (it != _rigidBodys.end()) {
			_rigidBodys.erase(it);
		}
	}

	void Update();

	void SetLocalInertiaTensor();

	void ResolveCollision(CollisionManifold& manifold);

	void ResolveInpulse(CollisionManifold& manifold);

	void ClearManifold() { _manifolds.clear(); }

	void ApplyWarmStarting();
};

#endif // !_PHYSICSSYSTEM_H_