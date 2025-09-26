//=====================================================================================
// PhysicsSystem.h
// Author:Kaito Aoki
// Date:2025/09/09
//=====================================================================================

#include "PhysicsSystem.h"

void PhysicsSystem::Update() {

	if (_colliders.size() < 2) return; //衝突判定するものが2つ未満ならスキップ


	//各Colliderの現在フレームの情報をリセット
	for (auto* col : _colliders) {
		col->BeginFrame();
	}

	for (size_t i = 0; i < _colliders.size() - 1; i++) {
		for (size_t j = i + 1; j < _colliders.size(); j++) {
			auto* colA = _colliders[i];
			auto* colB = _colliders[j];
			CollisionManifold manifold = colA->Collide(colB);

			if (manifold.hasCollision) {	//衝突した際

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

	//=====================================================================
	//物理演算
	//=====================================================================
	for (size_t i = 0; i < _colliders.size() - 1; i++) {
		for (size_t j = i + 1; j < _colliders.size(); j++) {
			auto* colA = _colliders[i];
			auto* colB = _colliders[j];

			// 衝突情報
			CollisionManifold manifold = colA->Collide(colB);

			if (!manifold.hasCollision) continue; // 衝突していない

			// RigidBody を持つかどうか
			auto* rbA = colA->GetOwner()->GetComponent<RigidBody>();
			auto* rbB = colB->GetOwner()->GetComponent<RigidBody>();
			if (!rbA && !rbB) continue; // 両方静的ならスキップ


			// 小さな隙間（slop）を残して過剰補正を防ぐ
			float slop = 0.05f;
			float percent = 0.8f; // 0.2～0.8の範囲で調整可能
			float correctionDepth = (std::max)(0.0f, manifold.penetrationDepth - slop);

			//有効質量の計算
			float invMassA = (rbA) ? rbA->_invMass : 0.0f;
			float invMassB = (rbB) ? rbB->_invMass : 0.0f;
			float invMassSum = invMassA + invMassB;


			if (invMassSum <= 0.0f) continue;

			// 押し戻し量
			KTVECTOR3 correction = manifold.normal * (correctionDepth / invMassSum) * percent;
			KTVECTOR3 totalCorrection = KTVECTOR3(0.0f, 0.0f, 0.0f);
			for(const auto& contact : manifold.contacts) {
				totalCorrection += manifold.normal * (contact.penetration / invMassSum) * percent;
			}
			totalCorrection = totalCorrection / (float)manifold.contacts.size();
			if (rbA) {
				colA->GetOwner()->_transform._position += totalCorrection * invMassA;
			}
			if (rbB) {
				colB->GetOwner()->_transform._position -= totalCorrection * invMassB;
			}

			KTVECTOR3 tempAngA = rbA ? rbA->_angularVelocity : KTVECTOR3(0.0f, 0.0f, 0.0f);
			KTVECTOR3 tempAngB = rbB ? rbB->_angularVelocity : KTVECTOR3(0.0f, 0.0f, 0.0f);

			for (const auto& contact : manifold.contacts) {
				KTVECTOR3 rA = contact.position - colA->GetOwner()->_transform._position;
				KTVECTOR3 rB = contact.position - colB->GetOwner()->_transform._position;

				//速度修正
				KTVECTOR3 vA = rbA ? rbA->_velocity + Cross(rbA->_angularVelocity, rA) : KTVECTOR3(0.0f, 0.0f, 0.0f);
				KTVECTOR3 vB = rbB ? rbB->_velocity + Cross(rbB->_angularVelocity, rB) : KTVECTOR3(0.0f, 0.0f, 0.0f);

				//KTVECTOR3 vA = rbA ? rbA->_velocity + Cross(tempAngA, rA) : KTVECTOR3(0.0f, 0.0f, 0.0f);
				//KTVECTOR3 vB = rbB ? rbB->_velocity + Cross(tempAngB, rB) : KTVECTOR3(0.0f, 0.0f, 0.0f);


				//相対速度
				KTVECTOR3 rV = vB - vA;
				float relVelAlongNormal = Dot(rV, manifold.normal);

				if (relVelAlongNormal >= 0.0f) { // 離れていく場合はスキップ
					//法線方向
					// 反発係数
					float e = 0.0f;
					if (rbA && rbB)
						e = (std::min)(rbA->_restitution, rbB->_restitution);
					else if (rbA)
						e = rbA->_restitution;
					else if (rbB)
						e = rbB->_restitution;

					//有効質量
					KTVECTOR3 rnA = rbA ? Cross(rA, manifold.normal) : KTVECTOR3(0.0f, 0.0f, 0.0f);
					KTVECTOR3 rnB = rbB ? Cross(rB, manifold.normal) : KTVECTOR3(0.0f, 0.0f, 0.0f);

					float angA = rbA ? Dot(rbA->_inertiaTensorWorldInv * rnA, rnA) : 0.0f;
					float angB = rbB ? Dot(rbB->_inertiaTensorWorldInv * rnB, rnB) : 0.0f;
					invMassSum = invMassA + invMassB + angA + angB;

					if (invMassSum <= 0.0f) continue;

					// 衝突インパルスの計算
					float j = -(1.0f + e) * relVelAlongNormal;
					j /= invMassSum;

					KTVECTOR3 impulse = j * manifold.normal;

					if (rbA) {
						rbA->_velocity -= (impulse * invMassA);
						rbA->_angularVelocity -= rbA->_inertiaTensorWorldInv * Cross(rA, impulse);
						//角速度加わるのやめてほしい
						if (rbA->_angularVelocity.Absolute() < 1e-1f) rbA->_angularVelocity = KTVECTOR3(0.0f, 0.0f, 0.0f);
					}
					if (rbB) {
						rbB->_velocity += (impulse * invMassB);
						rbB->_angularVelocity += rbB->_inertiaTensorWorldInv * Cross(rB, impulse);
						if (rbB->_angularVelocity.Absolute() < 1e-1f) rbB->_angularVelocity = KTVECTOR3(0.0f, 0.0f, 0.0f);
					}

					// 再計算
					//vA = rbA ? rbA->_velocity + Cross(tempAngA, rA) : KTVECTOR3(0.0f, 0.0f, 0.0f);
					//vB = rbB ? rbB->_velocity + Cross(tempAngB, rB) : KTVECTOR3(0.0f, 0.0f, 0.0f);

					vA = rbA ? rbA->_velocity + Cross(rbA->_angularVelocity, rA) : KTVECTOR3(0.0f, 0.0f, 0.0f);
					vB = rbB ? rbB->_velocity + Cross(rbB->_angularVelocity, rB) : KTVECTOR3(0.0f, 0.0f, 0.0f);
					rV = vB - vA;

					// 摩擦力の計算（接線方向）
					KTVECTOR3 tangent = rV - Dot(rV, manifold.normal) * manifold.normal;
					if (tangent.Absolute() > 1e-3f) {
						tangent = tangent.Normalize();

						// 接線方向の有効質量（回転寄与を含める）
						KTVECTOR3 rtA = rbA ? Cross(rA, tangent) : KTVECTOR3(0, 0, 0);
						KTVECTOR3 rtB = rbB ? Cross(rB, tangent) : KTVECTOR3(0, 0, 0);

						float tanAngA = rbA ? Dot(rbA->_inertiaTensorWorldInv * rtA, rtA) : 0.0f;
						float tanAngB = rbB ? Dot(rbB->_inertiaTensorWorldInv * rtB, rtB) : 0.0f;

						float denomTangent = invMassA + invMassB + tanAngA + tanAngB;
						if (denomTangent <= 0.0f) continue;

						float jt = -Dot(rV, tangent);
						jt /= denomTangent;

						// 静止摩擦と動摩擦の決定
						float mu_s = 0.0f;
						float mu_d = 0.0f;
						if (rbA && rbB) {
							mu_s = (std::max)(rbA->_staticFriction, rbB->_staticFriction);
							mu_d = (std::max)(rbA->_dynamicFriction, rbB->_dynamicFriction);
						}
						else if (rbA) {
							mu_s = rbA->_staticFriction;
							mu_d = rbA->_dynamicFriction;
						}
						else if (rbB) {
							mu_s = rbB->_staticFriction;
							mu_d = rbB->_dynamicFriction;
						}

						KTVECTOR3 frictionImpulse;
						if (fabs(jt) < j * mu_s)
							frictionImpulse = jt * tangent; // 静止摩擦
						else
							frictionImpulse = -j * mu_d * tangent; // 動摩擦

						if (rbA) {
							rbA->_velocity += (frictionImpulse * invMassA);
							rbA->_angularVelocity += rbA->_inertiaTensorWorldInv * Cross(rA, frictionImpulse);
						}
						if (rbB) {
							rbB->_velocity -= (frictionImpulse * invMassB);
							rbB->_angularVelocity -= rbB->_inertiaTensorWorldInv * Cross(rB, frictionImpulse);
						}
					}
				}

			}

			
		}
	}

	//剛体運動
	for (auto* col : _colliders) {
		auto* rb = col->GetOwner()->GetComponent<RigidBody>();
		if (rb && rb->GetActive()) {
			rb->Integrate();
		}
	}


	//状態更新
	for (auto* col : _colliders) {
		col->EndFrame();
	}




}