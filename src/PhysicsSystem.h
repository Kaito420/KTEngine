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

class PhysicsSystem {
	std::vector<Collider*> _colliders;
public:
	void RegisterCollider(Collider* collider) {
		_colliders.push_back(collider);
	}

	void Update() {
		for (int i = 0; i < _colliders.size() - 1; i++) {
			for (int j = i + 1; j < _colliders.size(); j++) {
				Collider* colA = _colliders[i];
				Collider* colB = _colliders[j];
				if(colA->IsOverlap((ColliderBox*)colB)) {	//¡‚ÍColliderBox*‚É‚µ‚Ä‚¢‚é‚ªA«—ˆ“I‚É‘¼‚ÌCollider‚É‚à‘Î‰ž‚³‚¹‚é
					colA->GetOwner()->DispatchCollision(colB);
					colB->GetOwner()->DispatchCollision(colA);
				}
			}
		}
	}
};

#endif // !_PHYSICSSYSTEM_H_