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
				if(colA->IsOverlap((ColliderBox*)colB)) {	//今はColliderBox*にしているが、将来的に他のColliderにも対応させる
					colA->_isOverlap = true;
					colB->_isOverlap = true;

					if (!colA->_wasOverlap)
						colA->GetOwner()->DispatchOnCollisionEnter(colB);
					else
						colA->GetOwner()->DispatchOnCollisionStay(colB);

					if (!colB->_wasOverlap)
						colB->GetOwner()->DispatchOnCollisionEnter(colA);
					else
						colB->GetOwner()->DispatchOnCollisionStay(colA);
				}
			}
		}

		//Exit判定
		for (auto* col : _colliders) {
			if (col->_wasOverlap && !col->_isOverlap) {
				col->GetOwner()->DispatchOnCollisionExit(nullptr);
			}
		}

		//状態更新
		for (auto* col : _colliders) {
			col->RestFrame();
		}
	}
};

#endif // !_PHYSICSSYSTEM_H_