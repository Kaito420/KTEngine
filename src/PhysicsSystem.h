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
#include "Profiler.h"

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

	static float _fixedDeltaTime; // 60Hz Fixed Update
	static int _subSteps;         // Sub-stepping count per FixedUpdate
	static int _solverIterations; // Sequential Impulse iterations per substep

	void StepSub(float subDt, PhysicsMetrics& metrics, bool isLastSubStep);

public:
	static float GetFixedDeltaTime() { return _fixedDeltaTime; }
	static void SetFixedDeltaTime(float dt) { if (dt > 0.001f) _fixedDeltaTime = dt; }

	static int GetSubSteps() { return _subSteps; }
	static void SetSubSteps(int steps) { _subSteps = (steps < 1) ? 1 : steps; }

	static int GetSolverIterations() { return _solverIterations; }
	static void SetSolverIterations(int iters) { _solverIterations = (iters < 1) ? 1 : iters; }

	void RegisterCollider(Collider* collider) {
		if (!collider) return;
		if (std::find(_colliders.begin(), _colliders.end(), collider) == _colliders.end()) {
			_colliders.push_back(collider);
		}
	}

	void RemoveCollider(Collider* collider) {
		auto it = std::find(_colliders.begin(), _colliders.end(), collider);
		if (it != _colliders.end()) {
			_colliders.erase(it);
		}
	}

	void RegisterRigidBody(RigidBody* rigidBody) {
		if (!rigidBody) return;
		if (std::find(_rigidBodys.begin(), _rigidBodys.end(), rigidBody) == _rigidBodys.end()) {
			_rigidBodys.push_back(rigidBody);
		}
	}

	void RemoveRigidBody(RigidBody* rigidBody) {
		auto it = std::find(_rigidBodys.begin(), _rigidBodys.end(), rigidBody);
		if (it != _rigidBodys.end()) {
			_rigidBodys.erase(it);
		}
	}

	void Step(float fixedDt);
	void Update() { Step(_fixedDeltaTime); }
	void UpdateEditor();

	void SyncColliders();

	void SetLocalInertiaTensor();

	void ResolveCollision(CollisionManifold& manifold);

	void ResolveInpulse(CollisionManifold& manifold, float dt);

	void ClearManifold() { _manifolds.clear(); }

	void ApplyWarmStarting();

	void RenderDebug();
};

#endif // !_PHYSICSSYSTEM_H_