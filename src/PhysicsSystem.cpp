//=====================================================================================
// PhysicsSystem.h
// Author:Kaito Aoki
// Date:2025/09/09
//=====================================================================================

#include "PhysicsSystem.h"
#include <algorithm>

void PhysicsSystem::Update() {

	if (_colliders.size() < 2) return; //衝突判定するものが2つ未満ならスキップ


	//各Colliderの現在フレームの情報をリセット
	for (auto* col : _colliders) {
		col->BeginFrame();
	}

	//x軸についてソート
	std::sort(_colliders.begin(), _colliders.end(),
		[](Collider* a, Collider* b) {
			return a->_aabb.min.x < b->_aabb.min.x;
		});

	//manifoldsのリセット
	ClearManifold();
	//慣性テンソルの更新
	SetLocalInertiaTensor();

	for (size_t i = 0; i < _colliders.size() - 1; i++) {
		for (size_t j = i + 1; j < _colliders.size(); j++) {
			auto* colA = _colliders[i];
			auto* colB = _colliders[j];

			//x軸で重なっていなかったらbreak
			if (colA->_aabb.max.x < colB->_aabb.min.x) {
				break;
			}

			CollisionManifold manifold;

			if (colA->Collide(colB, manifold)) {	//衝突した際

				colA->_isOverlap = true;//確認用
				colB->_isOverlap = true;

				colA->_currentOverlaps.insert(colB);
				colB->_currentOverlaps.insert(colA);

				if (colA->_previousOverlaps.find(colB) == colA->_previousOverlaps.end()) {
					colA->GetOwner()->DispatchOnCollisionEnter(colB);
					if (colA->GetOwner()->GetComponent<RigidBody>())
						colA->GetOwner()->GetComponent<RigidBody>()->WakeUp();
				}
				else
					colA->GetOwner()->DispatchOnCollisionStay(colB);

				if (colB->_previousOverlaps.find(colA) == colB->_previousOverlaps.end()) {
					colB->GetOwner()->DispatchOnCollisionEnter(colA);
					if (colB->GetOwner()->GetComponent<RigidBody>())
						colB->GetOwner()->GetComponent<RigidBody>()->WakeUp();
				}
				else
					colB->GetOwner()->DispatchOnCollisionStay(colA);
			
				_manifolds.push_back(manifold);	//manifoldの保存
			}
		}
	}

	//Exit判定
	for (auto* col : _colliders) {
		for (auto* prevCol : col->_previousOverlaps) {
			if (col->_currentOverlaps.find(prevCol) == col->_currentOverlaps.end()) {
				col->GetOwner()->DispatchOnCollisionExit(prevCol);
				if(col->GetOwner()->GetComponent<RigidBody>())
					col->GetOwner()->GetComponent<RigidBody>()->WakeUp();
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

	//=====================================================================
	//物理演算
	//=====================================================================

	for (auto& manifold : _manifolds) {

		for (int iter = 0; iter < 20; iter++) {
			ResolveInpulse(manifold);
		}
		ResolveCollision(manifold);
	}


	//状態更新
	for (auto* col : _colliders) {
		col->EndFrame();
	}

}

void PhysicsSystem::SetLocalInertiaTensor(){
	for (auto& col : _colliders) {
		RigidBody* rb = col->GetOwner()->GetComponent<RigidBody>();
		if (rb == nullptr)continue;
		if (rb->_mass > 0.0f) {
			rb->_inertiaTensorBody = col->ComputeLocalInertiaTensor(rb->_mass);
			rb->_inertiaTensorBodyInv = rb->_inertiaTensorBody.Inverse();
		}
		else {
			rb->_inertiaTensorBody = KTMATRIX3::Zero();
			rb->_inertiaTensorBodyInv = KTMATRIX3::Zero();
		}
	}
}

void PhysicsSystem::ResolveCollision(CollisionManifold& manifold)
{
	RigidBody* rbA = manifold.a->GetOwner()->GetComponent<RigidBody>();
	RigidBody* rbB = manifold.b->GetOwner()->GetComponent<RigidBody>();
	if (!rbA && !rbB) return; // 両方静的ならスキップ

	// 小さな隙間（slop）を残して過剰補正を防ぐ
	float slop = 0.05f;
	float percent = 0.4f; // 0.2～0.8の範囲で調整
	float depth = (std::max)(0.0f, manifold.penetrationDepth - slop);

	//有効質量の計算
	float invMassA = (rbA) ? rbA->_invMass : 0.0f;
	float invMassB = (rbB) ? rbB->_invMass : 0.0f;
	float invMassSum = invMassA + invMassB;

	if (invMassSum <= 0.0f) return;

	// 押し戻し量
	KTVECTOR3 correction = KTVECTOR3(0.0f, 0.0f, 0.0f);
	for (const auto& contact : manifold.contacts) {
		correction += manifold.normal * (contact.penetration / invMassSum);
	}
	correction = correction / (float)manifold.contacts.size();
	if (rbA) {
		manifold.a->GetOwner()->_transform._position += correction * invMassA * percent;
	}
	if (rbB) {
		manifold.b->GetOwner()->_transform._position -= correction * invMassB * percent;
	}
}

void PhysicsSystem::ResolveInpulse(CollisionManifold& manifold)
{
	RigidBody* rbA = manifold.a->GetOwner()->GetComponent<RigidBody>();
	RigidBody* rbB = manifold.b->GetOwner()->GetComponent<RigidBody>();
	if (!rbA && !rbB) return; // 両方静的ならスキップ

	//有効質量の計算
	float invMassA = (rbA) ? rbA->_invMass : 0.0f;
	float invMassB = (rbB) ? rbB->_invMass : 0.0f;
	float deltaImpulse = 0.0f;

	// 反発係数
	float e = 0.0f;
	if (rbA && rbB)
		e = (std::max)(rbA->_restitution, rbB->_restitution);
	else if (rbA)
		e = rbA->_restitution;
	else if (rbB)
		e = rbB->_restitution;

	for (auto& contact : manifold.contacts) {//法線方向

		KTVECTOR3 rA = contact.position - manifold.a->GetOwner()->_transform._position;
		KTVECTOR3 rB = contact.position - manifold.b->GetOwner()->_transform._position;

		//速度修正
		KTVECTOR3 vA = rbA ? rbA->_velocity + Cross(rbA->_angularVelocity, rA) : KTVECTOR3(0.0f, 0.0f, 0.0f);
		KTVECTOR3 vB = rbB ? rbB->_velocity + Cross(rbB->_angularVelocity, rB) : KTVECTOR3(0.0f, 0.0f, 0.0f);
		//相対速度
		KTVECTOR3 rV = vA - vB;//B->Aの相対速度
		float relVelAlongNormal = Dot(rV, manifold.normal);//manifold.normal => B->A方向
		if (relVelAlongNormal > 0.0f) continue;// 離れていく場合はスキップ(B->A方向で一致→内積が正の値の場合離れていく)


		//有効質量
		KTVECTOR3 rnA = rbA ? Cross(rA, manifold.normal) : KTVECTOR3(0.0f, 0.0f, 0.0f);
		KTVECTOR3 rnB = rbB ? Cross(rB, manifold.normal) : KTVECTOR3(0.0f, 0.0f, 0.0f);

		float angA = rbA ? Dot(rbA->_inertiaTensorWorldInv * rnA, rnA) : 0.0f;
		float angB = rbB ? Dot(rbB->_inertiaTensorWorldInv * rnB, rnB) : 0.0f;
		float normalMass = invMassA + invMassB + angA + angB;

		if (normalMass <= 0.0f) continue;

		// 衝突インパルスの計算（累積処理）
		//deltaImpulseの計算
		float deltaImpulse = -(1.0f + e) * relVelAlongNormal;
		deltaImpulse /= normalMass;


		//累積値の計算
		float oldSum = contact.normalImpulseSum;
		contact.normalImpulseSum += deltaImpulse;
		if(contact.normalImpulseSum < 0.0f)
			contact.normalImpulseSum = 0.0f;

		//実際に適用するのは差分だけ
		deltaImpulse = contact.normalImpulseSum - oldSum;

		//速度更新（インパルスの適用）
		KTVECTOR3 applyNormalImpule = deltaImpulse * manifold.normal;

		if (rbA && !rbA->IsSleeping()) {
			rbA->_velocity += (applyNormalImpule * invMassA);
			rbA->_angularVelocity += rbA->_inertiaTensorWorldInv * Cross(rA, applyNormalImpule);
		}
		if (rbB && !rbB->IsSleeping()) {
			rbB->_velocity -= (applyNormalImpule * invMassB);
			rbB->_angularVelocity -= rbB->_inertiaTensorWorldInv * Cross(rB, applyNormalImpule);
		}

	}
	
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

	for (auto& contact : manifold.contacts) {//接線方向
		//// 再計算
		KTVECTOR3 rA = contact.position - manifold.a->GetOwner()->_transform._position;
		KTVECTOR3 rB = contact.position - manifold.b->GetOwner()->_transform._position;

		//速度修正
		KTVECTOR3 vA = rbA ? rbA->_velocity + Cross(rbA->_angularVelocity, rA) : KTVECTOR3(0.0f, 0.0f, 0.0f);
		KTVECTOR3 vB = rbB ? rbB->_velocity + Cross(rbB->_angularVelocity, rB) : KTVECTOR3(0.0f, 0.0f, 0.0f);
		//相対速度
		KTVECTOR3 rV = vA - vB;

		// 摩擦力の計算（接線方向）
		KTVECTOR3 tangent = rV - Dot(rV, manifold.normal) * manifold.normal;
		if (tangent.Magnitude() > 1e-3f) {
			tangent = tangent.Normalize();

			// 接線方向の有効質量（回転寄与を含める）
			KTVECTOR3 rtA = rbA ? Cross(rA, tangent) : KTVECTOR3(0, 0, 0);
			KTVECTOR3 rtB = rbB ? Cross(rB, tangent) : KTVECTOR3(0, 0, 0);

			float tanAngA = rbA ? Dot(rbA->_inertiaTensorWorldInv * rtA, rtA) : 0.0f;
			float tanAngB = rbB ? Dot(rbB->_inertiaTensorWorldInv * rtB, rtB) : 0.0f;

			float tangentMass = invMassA + invMassB + tanAngA + tanAngB;
			if (tangentMass <= 0.0f) continue;

			float jt = -Dot(rV, tangent);
			jt /= tangentMass;


			//摩擦の上限値計算(F = μ * N)
			float maxFriction = contact.normalImpulseSum * mu_s;

			//累積値の計算
			KTVECTOR3 oldTangentImpulse = contact.tangentImpulseSum;
			KTVECTOR3 newTangentImpulse = oldTangentImpulse + jt * tangent;
			//円錐摩擦制約でクランプ
			float len = newTangentImpulse.Magnitude();
			if (len > maxFriction) {
				newTangentImpulse = (newTangentImpulse / len) * maxFriction;
			}
			contact.tangentImpulseSum = newTangentImpulse;
			//実際に適用するのは差分だけ
			KTVECTOR3 applyTangentImpulse = contact.tangentImpulseSum - oldTangentImpulse;

			if (rbA && !rbA->IsSleeping()) {
				rbA->_velocity += (applyTangentImpulse * invMassA);
				rbA->_angularVelocity += rbA->_inertiaTensorWorldInv * Cross(rA, applyTangentImpulse);
			}
			if (rbB && !rbB->IsSleeping()) {
				rbB->_velocity -= (applyTangentImpulse * invMassB);
				rbB->_angularVelocity -= rbB->_inertiaTensorWorldInv * Cross(rB, applyTangentImpulse);
			}
		}
	}
	rbA->CheckSleep();
	rbB->CheckSleep();
}
