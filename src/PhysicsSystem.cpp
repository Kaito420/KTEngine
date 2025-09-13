//=====================================================================================
// PhysicsSystem.h
// Author:Kaito Aoki
// Date:2025/09/09
//=====================================================================================

#include "PhysicsSystem.h"

void PhysicsSystem::Update() {
	//各Colliderの現在フレームの情報をリセット
	for (auto* col : _colliders) {
		col->BeginFrame();
	}

	for (size_t i = 0; i < _colliders.size() - 1; i++) {
		for (size_t j = i + 1; j < _colliders.size(); j++) {
			auto* colA = _colliders[i];
			auto* colB = _colliders[j];
			if (colA->IsOverlap(colB)) {	//衝突した際

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

			if (!colA->IsOverlap(colB)) continue; // 衝突していない

			// RigidBody を持つかどうか
			auto* rbA = colA->GetOwner()->GetComponent<RigidBody>();
			auto* rbB = colB->GetOwner()->GetComponent<RigidBody>();
			if (!rbA && !rbB) continue; // 両方静的ならスキップ

			// 衝突情報
			KTVECTOR3 normal = colA->_collisionInfo._collisionNormal;
			float depth = colA->_collisionInfo._penetrationDepth;
			KTVECTOR3 contactPoint = colA->_collisionInfo._collisionPoint;
			KTVECTOR3 rA = contactPoint - colA->GetOwner()->_transform._position;
			KTVECTOR3 rB = contactPoint - colB->GetOwner()->_transform._position;

			// 小さな隙間（slop）を残して過剰補正を防ぐ
			float slop = 0.01f;
			float correctionDepth = (std::max)(0.0f, depth - slop);

			//有効質量の計算
			float invMassA = (rbA) ? rbA->_invMass : 0.0f;
			float invMassB = (rbB) ? rbB->_invMass : 0.0f;

			KTVECTOR3 rnA = rbA ? Cross(rA, normal) : KTVECTOR3(0.0f, 0.0f, 0.0f);
			KTVECTOR3 rnB = rbB ? Cross(rB, normal) : KTVECTOR3(0.0f, 0.0f, 0.0f);

			float angA = rbA ? Dot(rbA->_inertiaTensorWorldInv * rnA, rnA) : 0.0f;
			float angB = rbB ? Dot(rbB->_inertiaTensorWorldInv * rnB, rnB) : 0.0f;

			float invMassSum = invMassA + invMassB + angA + angB;


			if (invMassSum <= 0.0f) continue;

			// 押し戻し量
			KTVECTOR3 correction = normal * (correctionDepth / invMassSum);

			if (rbA) {
				colA->GetOwner()->_transform._position -= correction * invMassA;
			}
			if (rbB) {
				colB->GetOwner()->_transform._position += correction * invMassB;
			}

			//速度修正

			KTVECTOR3 vA = rbA ? rbA->_velocity + Cross(rbA->_angularVelocity,rA) : KTVECTOR3(0.0f, 0.0f, 0.0f);
			KTVECTOR3 vB = rbB ? rbB->_velocity + Cross(rbB->_angularVelocity,rB) : KTVECTOR3(0.0f, 0.0f, 0.0f);

			//相対速度
			KTVECTOR3 rV = vB - vA;
			float relVelAlongNormal = Dot(rV, normal);

			if (relVelAlongNormal < 0.0f) { // 離れていく場合はスキップ
				//法線方向
				// 反発係数
				float e = 0.0f;
				if (rbA && rbB)
					e = (std::min)(rbA->_restitution, rbB->_restitution);
				else if (rbA)
					e = rbA->_restitution;
				else if (rbB)
					e = rbB->_restitution;

				// 衝突インパルスの計算
				float j = -(1.0f + e) * relVelAlongNormal;
				j /= invMassSum;

				KTVECTOR3 impulse = j * normal;

				if (rbA) {
					rbA->_velocity -= (impulse* invMassA);
					rbA->_angularVelocity -= rbA->_inertiaTensorWorldInv * Cross(rA, impulse);
					if(rbA->_velocity.Absolute() < 1e-2f)//微少であれば0にする
						rbA->_velocity = KTVECTOR3(0.0f, 0.0f, 0.0f);
				}
				if (rbB) {
					rbB->_velocity += (impulse * invMassB);
					rbB->_angularVelocity += rbB->_inertiaTensorWorldInv * Cross(rB, impulse);
					if(rbB->_velocity.Absolute() < 1e-2f)//微少であれば0にする
						rbB->_velocity = KTVECTOR3(0.0f, 0.0f, 0.0f);
				}

				// 摩擦力の計算（接線方向）
				KTVECTOR3 tangent = rV - Dot(rV, normal) * normal;
				if (tangent.Absolute() > 1e-6f) {
					tangent = tangent.Normalize();

					float jt = -Dot(rV, tangent);
					jt /= invMassSum;

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

					//クーロン摩擦制限
					float maxStaticFriction = mu_s * j;
					float maxDynamicFriction = mu_d * j;

					//摩擦の判定
					if (std::fabs(jt) < maxStaticFriction) {
						//静止摩擦
						if (rbA)
							rbA->_velocity = KTVECTOR3(0.0f, 0.0f, 0.0f);
						if (rbB)
							rbB->_velocity = KTVECTOR3(0.0f, 0.0f, 0.0f);
					}
					else {
						//動摩擦
						float jtFriction = -j * mu_d;
						if (jt < 0) jtFriction = -jtFriction;
						if (rbA) {
							rbA->_velocity += (jtFriction * invMassA) * tangent;
							if (rbA->_velocity.Absolute() < 1e-2f)//微少であれば0にする
								rbA->_velocity = KTVECTOR3(0.0f, 0.0f, 0.0f);
						}
						if (rbB) {
							rbB->_velocity -= (jtFriction * invMassB) * tangent;
							if (rbB->_velocity.Absolute() < 1e-2f)//微少であれば0にする
								rbB->_velocity = KTVECTOR3(0.0f, 0.0f, 0.0f);
						}
					}
				}
			}
		}
	}


	//状態更新
	for (auto* col : _colliders) {
		col->EndFrame();
	}

	//剛体運動
	for (auto* col : _colliders) {
				auto* rb = col->GetOwner()->GetComponent<RigidBody>();
		if (rb && rb->GetActive()) {
			rb->Integrate();
		}
	}


}