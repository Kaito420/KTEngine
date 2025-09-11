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

	void Update() {

		//各Colliderの現在フレームの情報をリセット
		for (auto* col : _colliders) {
			col->BeginFrame();
		}

		for (size_t i = 0; i < _colliders.size() - 1; i++) {
			for (size_t j = i + 1; j < _colliders.size(); j++) {
				auto* colA = _colliders[i];
				auto* colB = _colliders[j];
				if(colA->IsOverlap(colB)) {	//衝突した際
					//RigidBodyの処理を追加
					if (colA->GetOwner()->GetComponent<RigidBody>() && colB->GetOwner()->GetComponent<RigidBody>()) {
						RigidBody* rbA = colA->GetOwner()->GetComponent<RigidBody>();
						RigidBody* rbB = colB->GetOwner()->GetComponent<RigidBody>();

						GameObject::Transform* transformA = &colA->GetOwner()->_transform;
						//今はA側のめり込みを修正するだけだから使わない？
						GameObject::Transform* transformB = &colB->GetOwner()->_transform;

						//AとBの位置修正
						transformA->_position += colA->_collisionInfo._collisionNormal * colA->_collisionInfo._penetrationDepth;
						transformB->_position += colB->_collisionInfo._collisionNormal * colB->_collisionInfo._penetrationDepth;

						
					}

					colA->_isOverlap = true;//確認用
					colB->_isOverlap = true;

					colA->_currentOverlaps.insert(colB);
					colB->_currentOverlaps.insert(colA);

					if (colA->_previousOverlaps.find(colB) == colA->_previousOverlaps.end())
						colA->GetOwner()->DispatchOnCollisionEnter(colB);
					else
						colA->GetOwner()->DispatchOnCollisionStay(colB);

					if (colB->_previousOverlaps.find(colA) == colB->_previousOverlaps.end())
						colB->GetOwner()->DispatchOnCollisionEnter(colA);
					else
						colB->GetOwner()->DispatchOnCollisionStay(colA);
				}
			}
		}

		//Exit判定
		for (auto* col : _colliders) {
			for (auto* prevCol : col->_previousOverlaps) {
				if (col->_currentOverlaps.find(prevCol) == col->_currentOverlaps.end())
					col->GetOwner()->DispatchOnCollisionExit(prevCol);
			}
		}

		//状態更新
		for (auto* col : _colliders) {
			col->EndFrame();
		}
	}
};

#endif // !_PHYSICSSYSTEM_H_