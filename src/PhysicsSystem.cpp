//=====================================================================================
// PhysicsSystem.h
// Author:Kaito Aoki
// Date:2025/09/09
//=====================================================================================

#include "PhysicsSystem.h"
#include <algorithm>
#include <map>
#include <vector>
#include <cmath>
#include "Manager.h"
#include "ShaderManager.h"
#include "Renderer.h"

float PhysicsSystem::_fixedDeltaTime = 1.0f / 60.0f;
int PhysicsSystem::_subSteps = 2;
int PhysicsSystem::_solverIterations = 12;

void PhysicsSystem::SyncColliders() {
	for (auto* col : _colliders) {
		if (col && col->GetActive() && col->GetOwner() && col->GetOwner()->GetActive()) {
			col->SyncTransform();
		}
	}
}

void PhysicsSystem::Step(float fixedDt) {
	if (fixedDt <= 0.0f) return;

	PhysicsMetrics metrics;
	metrics.totalRigidBodies = (int)_rigidBodys.size();
	metrics.totalColliders = (int)_colliders.size();

	for (auto* rb : _rigidBodys) {
		if (rb && rb->GetActive() && rb->GetOwner() && rb->GetOwner()->GetActive()) {
			metrics.activeRigidBodies++;
		}
	}

	for (auto* col : _colliders) {
		if (col && col->GetActive() && col->GetOwner() && col->GetOwner()->GetActive()) {
			metrics.activeColliders++;
		}
	}

	int subSteps = (_subSteps > 0) ? _subSteps : 1;
	float subDt = fixedDt / (float)subSteps;

	for (int s = 0; s < subSteps; ++s) {
		bool isLastSubStep = (s == subSteps - 1);
		StepSub(subDt, metrics, isLastSubStep);
	}

	// FixedUpdate1蝗槫・縺ｮ蜈ｨ繧ｵ繝悶せ繝・ャ繝礼ｵゆｺ・ｾ後↓闢・ｩ榊鴨繧偵け繝ｪ繧｢
	for (auto* rb : _rigidBodys) {
		if (rb) rb->ClearForces();
	}

	Profiler::RecordPhysicsMetrics(metrics);
}

void PhysicsSystem::StepSub(float subDt, PhysicsMetrics& metrics, bool isLastSubStep) {
	// 1. Integrate
	{
		PROFILE_SCOPE("Physics: Integrate");
		for (auto* rb : _rigidBodys) {
			if (rb && rb->GetActive() && rb->GetOwner() && rb->GetOwner()->GetActive()) {
				rb->Integrate(subDt);
			}
		}
	}

	// 2. Sync Colliders (Integrate蠕後・譛譁ｰTransform縺ｧAABB縺ｨOBB霆ｸ繧呈峩譁ｰ)
	{
		PROFILE_SCOPE("Physics: Broadphase");
		SyncColliders();

		if (_colliders.size() < 2) {
			return;
		}

		for (auto* col : _colliders) {
			if (col) col->BeginFrame();
		}

		std::sort(_colliders.begin(), _colliders.end(),
			[](Collider* a, Collider* b) {
				if (!a || !b) return a < b;
				return a->_aabb.min.x < b->_aabb.min.x;
			});

		_prevManifolds = std::move(_manifolds);
		ClearManifold();
		SetLocalInertiaTensor();
	}

	// 3. Narrowphase Collision Detection
	{
		PROFILE_SCOPE("Physics: Narrowphase");
		for (size_t i = 0; i < _colliders.size() - 1; i++) {
			for (size_t j = i + 1; j < _colliders.size(); j++) {
				auto* colA = _colliders[i];
				auto* colB = _colliders[j];
				if (!colA || !colB) continue;
				if (!colA->GetActive() || !colB->GetActive()) continue;
				if (!colA->GetOwner() || !colB->GetOwner() || !colA->GetOwner()->GetActive() || !colB->GetOwner()->GetActive()) continue;

				metrics.broadphasePairs++;

				if (colA->_aabb.max.x < colB->_aabb.min.x) {
					break;
				}

				metrics.narrowphaseTests++;
				CollisionManifold manifold;

				if (colA->Collide(colB, manifold)) {
					colA->_isOverlap = true;
					colB->_isOverlap = true;

					colA->_currentOverlaps.insert(colB);
					colB->_currentOverlaps.insert(colA);

					if (colA->_previousOverlaps.find(colB) == colA->_previousOverlaps.end()) {
						colA->GetOwner()->DispatchOnCollisionEnter(colB);
						if (colA->GetOwner()->GetComponent<RigidBody>())
							colA->GetOwner()->GetComponent<RigidBody>()->WakeUp();
					}
					else if (isLastSubStep) {
						colA->GetOwner()->DispatchOnCollisionStay(colB);
					}

					if (colB->_previousOverlaps.find(colA) == colB->_previousOverlaps.end()) {
						colB->GetOwner()->DispatchOnCollisionEnter(colA);
						if (colB->GetOwner()->GetComponent<RigidBody>())
							colB->GetOwner()->GetComponent<RigidBody>()->WakeUp();
					}
					else if (isLastSubStep) {
						colB->GetOwner()->DispatchOnCollisionStay(colA);
					}

					_manifolds.push_back(manifold);
				}
			}
		}

		// Exit events
		for (auto* col : _colliders) {
			if (!col || !col->GetOwner()) continue;
			for (auto* prevCol : col->_previousOverlaps) {
				if (col->_currentOverlaps.find(prevCol) == col->_currentOverlaps.end()) {
					col->GetOwner()->DispatchOnCollisionExit(prevCol);
					if (col->GetOwner()->GetComponent<RigidBody>())
						col->GetOwner()->GetComponent<RigidBody>()->WakeUp();
				}
			}
		}
	}

	metrics.contactManifolds = (int)_manifolds.size();

	// 4. Solve Collisions & Impulses
	{
		PROFILE_SCOPE("Physics: Solve");
		for (auto& manifold : _manifolds) {
			RigidBody* rbA = manifold.a->GetOwner()->GetComponent<RigidBody>();
			RigidBody* rbB = manifold.b->GetOwner()->GetComponent<RigidBody>();

			float e = 0.0f;
			if (rbA && rbB) e = (std::max)(rbA->_restitution, rbB->_restitution);
			else if (rbA) e = rbA->_restitution;
			else if (rbB) e = rbB->_restitution;

			for (auto& cp : manifold.contacts) {
				KTVECTOR3 rA = cp.position - manifold.a->GetOwner()->GetWorldPosition();
				KTVECTOR3 rB = cp.position - manifold.b->GetOwner()->GetWorldPosition();

				KTVECTOR3 vA = rbA ? rbA->_velocity + Cross(rbA->_angularVelocity, rA) : KTVECTOR3(0, 0, 0);
				KTVECTOR3 vB = rbB ? rbB->_velocity + Cross(rbB->_angularVelocity, rB) : KTVECTOR3(0, 0, 0);
				KTVECTOR3 rv = vA - vB;
				float vRel = Dot(rv, manifold.normal);

				if (vRel <= -1.0f) {
					cp.velocityBias = -e * vRel;
				}
				else {
					cp.velocityBias = 0.0f;
				}
			}
		}

		ApplyWarmStarting();

		int iterations = (_solverIterations > 0) ? _solverIterations : 10;
		for (int iter = 0; iter < iterations; iter++) {
			for (auto& manifold : _manifolds) {
				ResolveInpulse(manifold, subDt);
			}
		}

		for (auto& manifold : _manifolds) {
			ResolveCollision(manifold);
		}

		for (auto* rb : _rigidBodys) {
			if (rb && rb->GetActive() && rb->GetOwner() && rb->GetOwner()->GetActive() && !rb->_isKinematic) {
				rb->CheckSleep(subDt);
			}
		}

		// 菴咲ｽｮ陬懈ｭ｣蠕後・蠎ｧ讓吶ｒ繧ｳ繝ｩ繧､繝繝ｼ縺ｮAABB繝ｻ霆ｸ縺ｸ蜀榊酔譛・
		SyncColliders();

		for (auto* col : _colliders) {
			if (col) col->EndFrame();
		}
	}
}

void PhysicsSystem::UpdateEditor() {
	PhysicsMetrics metrics;
	metrics.totalRigidBodies = (int)_rigidBodys.size();
	metrics.totalColliders = (int)_colliders.size();

	for (auto* col : _colliders) {
		if (col && col->GetActive() && col->GetOwner() && col->GetOwner()->GetActive()) {
			metrics.activeColliders++;
			col->SyncTransform();
		}
	}

	if (_colliders.size() < 2) {
		for (auto* col : _colliders) {
			if (col) col->EndFrame();
		}
		Profiler::RecordPhysicsMetrics(metrics);
		return;
	}

	// Broadphase & Sort
	{
		for (auto* col : _colliders) {
			if (col) col->BeginFrame();
		}

		std::sort(_colliders.begin(), _colliders.end(),
			[](Collider* a, Collider* b) {
				if (!a || !b) return a < b;
				return a->_aabb.min.x < b->_aabb.min.x;
			});

		ClearManifold();
	}

	// Narrowphase Collision Detection
	{
		for (size_t i = 0; i < _colliders.size() - 1; i++) {
			for (size_t j = i + 1; j < _colliders.size(); j++) {
				auto* colA = _colliders[i];
				auto* colB = _colliders[j];
				if (!colA || !colB) continue;
				if (!colA->GetActive() || !colB->GetActive()) continue;
				if (!colA->GetOwner() || !colB->GetOwner() || !colA->GetOwner()->GetActive() || !colB->GetOwner()->GetActive()) continue;

				metrics.broadphasePairs++;

				if (colA->_aabb.max.x < colB->_aabb.min.x) {
					break;
				}

				metrics.narrowphaseTests++;
				CollisionManifold manifold;

				if (colA->Collide(colB, manifold)) {
					colA->_isOverlap = true;
					colB->_isOverlap = true;
					colA->_currentOverlaps.insert(colB);
					colB->_currentOverlaps.insert(colA);
					_manifolds.push_back(manifold);
				}
			}
		}

		for (auto* col : _colliders) {
			if (col) col->EndFrame();
		}
	}

	metrics.contactManifolds = (int)_manifolds.size();
	Profiler::RecordPhysicsMetrics(metrics);
}

void PhysicsSystem::SetLocalInertiaTensor(){
	for (auto& col : _colliders) {
		if (!col || !col->GetOwner()) continue;
		RigidBody* rb = col->GetOwner()->GetComponent<RigidBody>();
		if (rb == nullptr)continue;
		bool massChanged = ((rb->_oldMass - rb->_mass) * (rb->_oldMass - rb->_mass) > 1e-6f);
		if (!col->_hasChangedScale && !massChanged) continue; //繧ｹ繧ｱ繝ｼ繝ｫ縺玖ｳｪ驥上′螟牙喧縺励※縺・↑縺代ｌ縺ｰ繧ｹ繧ｭ繝・・
		if (rb->_mass > 0.0f && !rb->_isKinematic) {
			rb->_inertiaTensorBody = col->ComputeLocalInertiaTensor(rb->_mass);
			rb->_inertiaTensorBodyInv = rb->_inertiaTensorBody.Inverse();
		}
		else {
			rb->_inertiaTensorBody = KTMATRIX3::Zero();
			rb->_inertiaTensorBodyInv = KTMATRIX3::Zero();
		}
		rb->_oldMass = rb->_mass;
		col->_hasChangedScale = false;
	}
}

void PhysicsSystem::ResolveCollision(CollisionManifold& manifold)
{
	RigidBody* rbA = manifold.a->GetOwner()->GetComponent<RigidBody>();
	RigidBody* rbB = manifold.b->GetOwner()->GetComponent<RigidBody>();
	if (!rbA && !rbB) return; // 荳｡譁ｹ髱咏噪縺ｪ繧峨せ繧ｭ繝・・

	// 蟆上＆縺ｪ髫咎俣・・lop・峨ｒ谿九＠縺ｦ驕主臆陬懈ｭ｣繧帝亟縺・
	float slop = 0.02f;
	float percent = 0.4f; // 0.2・・.8縺ｮ遽・峇縺ｧ隱ｿ謨ｴ

	//譛牙柑雉ｪ驥上・險育ｮ・
	float invMassA = (rbA && !rbA->IsSleeping()) ? rbA->_invMass : 0.0f;
	float invMassB = (rbB && !rbB->IsSleeping()) ? rbB->_invMass : 0.0f;
	float invMassSum = invMassA + invMassB;

	if (invMassSum <= 0.0f || manifold.contacts.empty()) return;

	// 謚ｼ縺玲綾縺鈴㍼・・lop 繧貞ｷｮ縺怜ｼ輔＞縺溘ａ繧願ｾｼ縺ｿ驥上〒陬懈ｭ｣・・
	KTVECTOR3 correction = KTVECTOR3(0.0f, 0.0f, 0.0f);
	for (const auto& contact : manifold.contacts) {
		float pen = (std::max)(0.0f, contact.penetration - slop);
		correction += manifold.normal * (pen / invMassSum);
	}
	correction = correction / (float)manifold.contacts.size();
	if (rbA && !rbA->IsSleeping()) {
		manifold.a->GetOwner()->SetWorldPosition(manifold.a->GetOwner()->GetWorldPosition() + correction * invMassA * percent);
	}
	if (rbB && !rbB->IsSleeping()) {
		manifold.b->GetOwner()->SetWorldPosition(manifold.b->GetOwner()->GetWorldPosition() - correction * invMassB * percent);
	}
}

void PhysicsSystem::ResolveInpulse(CollisionManifold& manifold, float dt)
{
	RigidBody* rbA = manifold.a->GetOwner()->GetComponent<RigidBody>();
	RigidBody* rbB = manifold.b->GetOwner()->GetComponent<RigidBody>();
	if (!rbA && !rbB) return; // 荳｡譁ｹ髱咏噪縺ｪ繧峨せ繧ｭ繝・・

	//譛牙柑雉ｪ驥上・險育ｮ・
	float invMassA = (rbA) ? rbA->_invMass : 0.0f;
	float invMassB = (rbB) ? rbB->_invMass : 0.0f;

	for (auto& contact : manifold.contacts) {//豕慕ｷ壽婿蜷・

		KTVECTOR3 rA = contact.position - manifold.a->GetOwner()->GetWorldPosition();
		KTVECTOR3 rB = contact.position - manifold.b->GetOwner()->GetWorldPosition();

		//騾溷ｺｦ菫ｮ豁｣
		KTVECTOR3 vA = rbA ? rbA->_velocity + Cross(rbA->_angularVelocity, rA) : KTVECTOR3(0.0f, 0.0f, 0.0f);
		KTVECTOR3 vB = rbB ? rbB->_velocity + Cross(rbB->_angularVelocity, rB) : KTVECTOR3(0.0f, 0.0f, 0.0f);
		//逶ｸ蟇ｾ騾溷ｺｦ
		KTVECTOR3 rV = vA - vB;//B->A縺ｮ逶ｸ蟇ｾ騾溷ｺｦ
		float relVelAlongNormal = Dot(rV, manifold.normal);//manifold.normal => B->A譁ｹ蜷・
		// 髮｢繧後※縺・￥蝣ｴ蜷医・繧ｹ繧ｭ繝・・(B->A譁ｹ蜷代〒荳閾ｴ竊貞・遨阪′豁｣縺ｮ蛟､縺ｮ蝣ｴ蜷磯屬繧後※縺・￥) && 豕慕ｷ壹う繝ｳ繝代Ν繧ｹ縺・莉･荳九・蝣ｴ蜷医せ繧ｭ繝・・
		if (relVelAlongNormal > 0.0f && contact.normalImpulseSum <= 0.0f)continue;

		//譛牙柑雉ｪ驥・
		KTVECTOR3 rnA = rbA ? Cross(rA, manifold.normal) : KTVECTOR3(0.0f, 0.0f, 0.0f);
		KTVECTOR3 rnB = rbB ? Cross(rB, manifold.normal) : KTVECTOR3(0.0f, 0.0f, 0.0f);

		float angA = rbA ? Dot(rbA->_inertiaTensorWorldInv * rnA, rnA) : 0.0f;
		float angB = rbB ? Dot(rbB->_inertiaTensorWorldInv * rnB, rnB) : 0.0f;
		float normalMass = invMassA + invMassB + angA + angB;

		if (normalMass <= 0.0f) continue;

		// 陦晉ｪ√う繝ｳ繝代Ν繧ｹ縺ｮ險育ｮ暦ｼ育ｴｯ遨榊・逅・ｼ・
		float deltaImpulse = (contact.velocityBias - relVelAlongNormal) / normalMass;

		//邏ｯ遨榊､縺ｮ險育ｮ・
		float oldSum = contact.normalImpulseSum;
		contact.normalImpulseSum += deltaImpulse;
		if(contact.normalImpulseSum < 0.0f)
			contact.normalImpulseSum = 0.0f;

		//螳滄圀縺ｫ驕ｩ逕ｨ縺吶ｋ縺ｮ縺ｯ蟾ｮ蛻・□縺・
		deltaImpulse = contact.normalImpulseSum - oldSum;

		//騾溷ｺｦ譖ｴ譁ｰ・医う繝ｳ繝代Ν繧ｹ縺ｮ驕ｩ逕ｨ・・
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
	
	// 髱呎ｭ｢鞫ｩ謫ｦ縺ｨ蜍墓束謫ｦ縺ｮ豎ｺ螳・
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

	for (auto& contact : manifold.contacts) {//謗･邱壽婿蜷・
		KTVECTOR3 rA = contact.position - manifold.a->GetOwner()->GetWorldPosition();
		KTVECTOR3 rB = contact.position - manifold.b->GetOwner()->GetWorldPosition();

		//騾溷ｺｦ菫ｮ豁｣
		KTVECTOR3 vA = rbA ? rbA->_velocity + Cross(rbA->_angularVelocity, rA) : KTVECTOR3(0.0f, 0.0f, 0.0f);
		KTVECTOR3 vB = rbB ? rbB->_velocity + Cross(rbB->_angularVelocity, rB) : KTVECTOR3(0.0f, 0.0f, 0.0f);
		//逶ｸ蟇ｾ騾溷ｺｦ
		KTVECTOR3 rV = vA - vB;

		// 鞫ｩ謫ｦ蜉帙・險育ｮ暦ｼ域磁邱壽婿蜷托ｼ・
		KTVECTOR3 tangent = rV - Dot(rV, manifold.normal) * manifold.normal;
		if (tangent.Magnitude() > 1e-3f) {
			tangent = tangent.Normalize();

			// 謗･邱壽婿蜷代・譛牙柑雉ｪ驥擾ｼ亥屓霆｢蟇・ｸ弱ｒ蜷ｫ繧√ｋ・・
			KTVECTOR3 rtA = rbA ? Cross(rA, tangent) : KTVECTOR3(0, 0, 0);
			KTVECTOR3 rtB = rbB ? Cross(rB, tangent) : KTVECTOR3(0, 0, 0);

			float tanAngA = rbA ? Dot(rbA->_inertiaTensorWorldInv * rtA, rtA) : 0.0f;
			float tanAngB = rbB ? Dot(rbB->_inertiaTensorWorldInv * rtB, rtB) : 0.0f;

			float tangentMass = invMassA + invMassB + tanAngA + tanAngB;
			if (tangentMass <= 0.0f) continue;

			float jt = -Dot(rV, tangent);
			jt /= tangentMass;

			// 髱呎ｭ｢鞫ｩ謫ｦ繝ｻ蜍墓束謫ｦ縺ｮ荳企剞蛟､險育ｮ・(F_s = ﾎｼ_s * N, F_d = ﾎｼ_d * N)
			float maxStaticFriction = contact.normalImpulseSum * mu_s;
			float maxDynamicFriction = contact.normalImpulseSum * mu_d;

			// 邏ｯ遨榊､縺ｮ險育ｮ・
			KTVECTOR3 oldTangentImpulse = contact.tangentImpulseSum;
			KTVECTOR3 newTangentImpulse = oldTangentImpulse + jt * tangent;
			// 蜀・倹鞫ｩ謫ｦ蛻ｶ邏・ｼ夐撕豁｢鞫ｩ謫ｦ荳企剞繧定ｶ・∴縺溘ｉ蜍墓束謫ｦ菫よ焚縺ｧ繧ｯ繝ｩ繝ｳ繝・
			float len = newTangentImpulse.Magnitude();
			if (len > maxStaticFriction && len > 1e-6f) {
				newTangentImpulse = (newTangentImpulse / len) * maxDynamicFriction;
			}
			contact.tangentImpulseSum = newTangentImpulse;
			// 螳滄圀縺ｫ驕ｩ逕ｨ縺吶ｋ縺ｮ縺ｯ蟾ｮ蛻・□縺・
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
}

void PhysicsSystem::ApplyWarmStarting(){

	static std::vector<ManifoldKey> searchList;
	searchList.clear();
	searchList.reserve(_prevManifolds.size());	//蜑阪ヵ繝ｬ繝ｼ繝縺ｮ繝槭ル繝輔か繝ｼ繝ｫ繝峨→蜷後し繧､繧ｺ蛻・

	//繝ｪ繧ｹ繝井ｽ懈・
	for(auto& old : _prevManifolds){
		uint64_t key = MakePairKey(old.a, old.b);
		searchList.push_back({ key, &old });
	}

	//繧ｽ繝ｼ繝・
	std::sort(searchList.begin(), searchList.end());

	constexpr float WARM_START_FACTOR = 0.8f;

	for (auto& newManifold : _manifolds) {
		//蜑阪ヵ繝ｬ繝ｼ繝縺ｮ蜷後§繝壹い縺ｮ繝槭ル繝輔か繝ｼ繝ｫ繝峨ｒ謗｢縺・
		uint64_t key = MakePairKey(newManifold.a, newManifold.b);

		//莠悟・謗｢邏｢
		ManifoldKey target = { key, nullptr };
		auto it = std::lower_bound(searchList.begin(), searchList.end(), target);

		if (it != searchList.end() && it->key == key) {//隕九▽縺九▲縺溷ｴ蜷・
			CollisionManifold* oldManifold = it->manifold;

			//蜑帑ｽ薙・蜿門ｾ・
			RigidBody* rbA = newManifold.a->GetOwner()->GetComponent<RigidBody>();
			RigidBody* rbB = newManifold.b->GetOwner()->GetComponent<RigidBody>();
			if (!rbA && !rbB) continue;

			//謗･隗ｦ轤ｹ縺ｮ繝槭ャ繝√Φ繧ｰ
			for (auto& newContact : newManifold.contacts) {

				float minDistSqr = FLT_MAX;
				ContactPoint* bestOldContact = nullptr;

				for (auto& oldContact : oldManifold->contacts) {
					float distSqr = (newContact.position - oldContact.position).MagnitudeSqr();

					//霍晞屬豈碑ｼ・
					if (distSqr < 0.005f && distSqr < minDistSqr) {
						minDistSqr = distSqr;
						bestOldContact = &oldContact;
					}
				}

				//繝槭ャ繝√☆繧句商縺・せ縺瑚ｦ九▽縺九▲縺溘ｉ邏ｯ遨阪う繝ｳ繝代Ν繧ｹ繧貞ｼ輔″邯吶＄・磯℃蜑ｰ闢・ｩ埼亟豁｢縺ｮ縺溘ａ貂幄｡ｰ菫よ焚繧呈寺縺代ｋ・・
				if (bestOldContact) {
					newContact.normalImpulseSum = bestOldContact->normalImpulseSum * WARM_START_FACTOR;
					newContact.tangentImpulseSum = bestOldContact->tangentImpulseSum * WARM_START_FACTOR;

					//蠑輔″邯吶＞縺繧､繝ｳ繝代Ν繧ｹ繧剃ｺ句燕縺ｫ騾溷ｺｦ縺ｫ驕ｩ逕ｨ
					KTVECTOR3 rA = newContact.position - newManifold.a->GetOwner()->GetWorldPosition();
					KTVECTOR3 rB = newContact.position - newManifold.b->GetOwner()->GetWorldPosition();

					//豕慕ｷ壽婿蜷代・繧､繝ｳ繝代Ν繧ｹ蠕ｩ蜈・
					KTVECTOR3 applyImpulse = newContact.normalImpulseSum * newManifold.normal;
					//謗･邱壽婿蜷代・繧､繝ｳ繝代Ν繧ｹ蠕ｩ蜈・
					applyImpulse += newContact.tangentImpulseSum;

					float invMassA = (rbA) ? rbA->_invMass : 0.0f;
					float invMassB = (rbB) ? rbB->_invMass : 0.0f;

					if (rbA && !rbA->IsSleeping()) {
						rbA->_velocity += applyImpulse * invMassA;
						rbA->_angularVelocity += rbA->_inertiaTensorWorldInv * Cross(rA, applyImpulse);
					}
					if (rbB && !rbB->IsSleeping()) {
						rbB->_velocity -= applyImpulse * invMassB;
						rbB->_angularVelocity -= rbB->_inertiaTensorWorldInv * Cross(rB, applyImpulse);
					}
				}
			}
		}
	}
}


uint64_t PhysicsSystem::MakePairKey(Collider* a, Collider* b) {
	if (a > b) std::swap(a, b);
	return (uint64_t)a ^ ((uint64_t)b << 32);
}

void PhysicsSystem::RenderDebug() {
	if (!Manager::IsShowColliderWireframe()) return;

	auto cmdList = Renderer::GetCommandListDX12();
	if (!cmdList) return;

	// 1. Static Wireframe Buffers Initialization
	static std::unique_ptr<VERTEX_BUFFER> s_boxVB = nullptr;
	static std::unique_ptr<INDEX_BUFFER> s_boxIB = nullptr;

	static std::unique_ptr<VERTEX_BUFFER> s_sphereVB = nullptr;
	static std::unique_ptr<INDEX_BUFFER> s_sphereIB = nullptr;
	static int s_sphereIndexCount = 0;

	static std::unique_ptr<VERTEX_BUFFER> s_capsuleVB = nullptr;
	static constexpr int CAPSULE_MAX_VERTS = 256;

	if (!s_boxVB) {
		// Unit Box [-0.5, 0.5]
		Vertex boxVerts[8] = {
			{ { -0.5f,  0.5f,  0.5f }, {0,0,0}, {1,1,1,1}, {0,0} },
			{ {  0.5f,  0.5f,  0.5f }, {0,0,0}, {1,1,1,1}, {0,0} },
			{ {  0.5f,  0.5f, -0.5f }, {0,0,0}, {1,1,1,1}, {0,0} },
			{ { -0.5f,  0.5f, -0.5f }, {0,0,0}, {1,1,1,1}, {0,0} },
			{ { -0.5f, -0.5f,  0.5f }, {0,0,0}, {1,1,1,1}, {0,0} },
			{ {  0.5f, -0.5f,  0.5f }, {0,0,0}, {1,1,1,1}, {0,0} },
			{ {  0.5f, -0.5f, -0.5f }, {0,0,0}, {1,1,1,1}, {0,0} },
			{ { -0.5f, -0.5f, -0.5f }, {0,0,0}, {1,1,1,1}, {0,0} }
		};
		unsigned int boxIndices[24] = {
			0,1, 1,2, 2,3, 3,0,
			4,5, 5,6, 6,7, 7,4,
			0,4, 1,5, 2,6, 3,7
		};

		s_boxVB = Renderer::CreateVertexBuffer(sizeof(Vertex), 8);
		void* data = nullptr;
		if (SUCCEEDED(s_boxVB->Resource->Map(0, nullptr, &data))) {
			memcpy(data, boxVerts, sizeof(boxVerts));
			s_boxVB->Resource->Unmap(0, nullptr);
		}

		s_boxIB = Renderer::CreateIndexBuffer(24);
		if (SUCCEEDED(s_boxIB->Resource->Map(0, nullptr, &data))) {
			memcpy(data, boxIndices, sizeof(boxIndices));
			s_boxIB->Resource->Unmap(0, nullptr);
		}
	}

	if (!s_sphereVB) {
		// 3 Orthogonal circles of unit radius 1.0 (XY, XZ, YZ)
		constexpr int SEGS = 24;
		std::vector<Vertex> sphereVerts;
		std::vector<unsigned int> sphereIndices;
		sphereVerts.reserve(SEGS * 3);
		sphereIndices.reserve(SEGS * 6);

		auto addCircle = [&](int plane) {
			// plane: 0 = XY, 1 = XZ, 2 = YZ
			unsigned int baseIdx = (unsigned int)sphereVerts.size();
			for (int i = 0; i < SEGS; i++) {
				float theta = (float)i * 2.0f * 3.14159265f / (float)SEGS;
				float c = cosf(theta);
				float s = sinf(theta);

				XMFLOAT3 pos;
				if (plane == 0) pos = XMFLOAT3(c, s, 0.0f);
				else if (plane == 1) pos = XMFLOAT3(c, 0.0f, s);
				else pos = XMFLOAT3(0.0f, c, s);

				sphereVerts.push_back({ pos, {0,0,0}, {1,1,1,1}, {0,0} });

				unsigned int next = (i + 1) % SEGS;
				sphereIndices.push_back(baseIdx + i);
				sphereIndices.push_back(baseIdx + next);
			}
		};

		addCircle(0); // XY
		addCircle(1); // XZ
		addCircle(2); // YZ

		s_sphereIndexCount = (int)sphereIndices.size();

		s_sphereVB = Renderer::CreateVertexBuffer(sizeof(Vertex), (UINT)sphereVerts.size());
		void* data = nullptr;
		if (SUCCEEDED(s_sphereVB->Resource->Map(0, nullptr, &data))) {
			memcpy(data, sphereVerts.data(), sizeof(Vertex) * sphereVerts.size());
			s_sphereVB->Resource->Unmap(0, nullptr);
		}

		s_sphereIB = Renderer::CreateIndexBuffer((UINT)sphereIndices.size());
		if (SUCCEEDED(s_sphereIB->Resource->Map(0, nullptr, &data))) {
			memcpy(data, sphereIndices.data(), sizeof(unsigned int) * sphereIndices.size());
			s_sphereIB->Resource->Unmap(0, nullptr);
		}
	}

	if (!s_capsuleVB) {
		s_capsuleVB = Renderer::CreateVertexBuffer(sizeof(Vertex), CAPSULE_MAX_VERTS);
	}

	// 2. Bind Line PSO
	ID3D12PipelineState* pso = ShaderManager::Instance().GetPipelineState(
		"UnlitColorVS", "UnlitColorPS", 1,
		D3D12_CULL_MODE_NONE,
		true,
		false,
		D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE);
	if (!pso) return;

	cmdList->SetPipelineState(pso);
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);

	// 3. Render Each Collider
	for (auto* col : _colliders) {
		if (!col || !col->GetActive() || !col->GetOwner() || !col->GetOwner()->GetActive()) continue;

		// Set color: Red if overlapping, Green if normal
		MATERIAL material = {};
		if (col->_wasOverlap || col->_isOverlap) {
			material.Diffuse = XMFLOAT4(1.0f, 0.2f, 0.2f, 1.0f); // Bright Red
		} else {
			material.Diffuse = XMFLOAT4(0.2f, 1.0f, 0.3f, 1.0f); // Bright Green
		}
		material.TextureEnable = false;
		Renderer::SetConstant(3, &material, sizeof(material));

		auto owner = col->GetOwner();
		XMMATRIX translation = XMMatrixTranslation(owner->GetWorldPosition().x, owner->GetWorldPosition().y, owner->GetWorldPosition().z);
		XMFLOAT4 q = XMFLOAT4(owner->GetWorldQuaternion().x, owner->GetWorldQuaternion().y, owner->GetWorldQuaternion().z, owner->GetWorldQuaternion().w);
		XMMATRIX rotation = XMMatrixRotationQuaternion(XMLoadFloat4(&q));

		if (auto* box = dynamic_cast<ColliderBox*>(col)) {
			// Box wireframe
			XMMATRIX scale = XMMatrixScaling(box->_extents.x * 2.0f, box->_extents.y * 2.0f, box->_extents.z * 2.0f);
			XMMATRIX worldMatrix = scale * rotation * translation;
			Renderer::SetWorldMatrix(worldMatrix);
			Renderer::BindShaderConstantsDX12();

			D3D12_VERTEX_BUFFER_VIEW vbView = {};
			vbView.BufferLocation = s_boxVB->Resource->GetGPUVirtualAddress();
			vbView.StrideInBytes = s_boxVB->Stride;
			vbView.SizeInBytes = s_boxVB->Stride * s_boxVB->Size;
			cmdList->IASetVertexBuffers(0, 1, &vbView);

			D3D12_INDEX_BUFFER_VIEW ibView = {};
			ibView.BufferLocation = s_boxIB->Resource->GetGPUVirtualAddress();
			ibView.SizeInBytes = sizeof(unsigned int) * s_boxIB->Size;
			ibView.Format = DXGI_FORMAT_R32_UINT;
			cmdList->IASetIndexBuffer(&ibView);

			cmdList->DrawIndexedInstanced(24, 1, 0, 0, 0);
		}
		else if (auto* sphere = dynamic_cast<ColliderSphere*>(col)) {
			// Sphere wireframe
			XMMATRIX scale = XMMatrixScaling(sphere->_radius, sphere->_radius, sphere->_radius);
			XMMATRIX worldMatrix = scale * rotation * translation;
			Renderer::SetWorldMatrix(worldMatrix);
			Renderer::BindShaderConstantsDX12();

			D3D12_VERTEX_BUFFER_VIEW vbView = {};
			vbView.BufferLocation = s_sphereVB->Resource->GetGPUVirtualAddress();
			vbView.StrideInBytes = s_sphereVB->Stride;
			vbView.SizeInBytes = s_sphereVB->Stride * s_sphereVB->Size;
			cmdList->IASetVertexBuffers(0, 1, &vbView);

			D3D12_INDEX_BUFFER_VIEW ibView = {};
			ibView.BufferLocation = s_sphereIB->Resource->GetGPUVirtualAddress();
			ibView.SizeInBytes = sizeof(unsigned int) * s_sphereIB->Size;
			ibView.Format = DXGI_FORMAT_R32_UINT;
			cmdList->IASetIndexBuffer(&ibView);

			cmdList->DrawIndexedInstanced((UINT)s_sphereIndexCount, 1, 0, 0, 0);
		}
		else if (auto* capsule = dynamic_cast<ColliderCapsule*>(col)) {
			// Capsule wireframe (Dynamic lines)
			float r = capsule->GetRadius();
			float h = (std::max)(0.0f, capsule->GetHeight() - 2.0f * r);
			float halfH = h * 0.5f;

			std::vector<Vertex> capVerts;
			constexpr int SEGS = 16;

			// Helper to add circle at Y offset
			auto addHCircle = [&](float y) {
				for (int i = 0; i < SEGS; i++) {
					float theta1 = (float)i * 2.0f * 3.14159265f / (float)SEGS;
					float theta2 = (float)(i + 1) * 2.0f * 3.14159265f / (float)SEGS;
					capVerts.push_back({ XMFLOAT3(r * cosf(theta1), y, r * sinf(theta1)), {0,0,0}, {1,1,1,1}, {0,0} });
					capVerts.push_back({ XMFLOAT3(r * cosf(theta2), y, r * sinf(theta2)), {0,0,0}, {1,1,1,1}, {0,0} });
				}
			};

			addHCircle(+halfH); // Top rim
			addHCircle(-halfH); // Bottom rim

			// 4 Cylinder side lines
			capVerts.push_back({ XMFLOAT3(+r, +halfH, 0), {0,0,0}, {1,1,1,1}, {0,0} });
			capVerts.push_back({ XMFLOAT3(+r, -halfH, 0), {0,0,0}, {1,1,1,1}, {0,0} });
			capVerts.push_back({ XMFLOAT3(-r, +halfH, 0), {0,0,0}, {1,1,1,1}, {0,0} });
			capVerts.push_back({ XMFLOAT3(-r, -halfH, 0), {0,0,0}, {1,1,1,1}, {0,0} });
			capVerts.push_back({ XMFLOAT3(0, +halfH, +r), {0,0,0}, {1,1,1,1}, {0,0} });
			capVerts.push_back({ XMFLOAT3(0, -halfH, +r), {0,0,0}, {1,1,1,1}, {0,0} });
			capVerts.push_back({ XMFLOAT3(0, +halfH, -r), {0,0,0}, {1,1,1,1}, {0,0} });
			capVerts.push_back({ XMFLOAT3(0, -halfH, -r), {0,0,0}, {1,1,1,1}, {0,0} });

			// Top dome arch (XY & ZY)
			for (int i = 0; i < SEGS / 2; i++) {
				float t1 = (float)i * 3.14159265f / (float)(SEGS / 2);
				float t2 = (float)(i + 1) * 3.14159265f / (float)(SEGS / 2);
				// XY arch
				capVerts.push_back({ XMFLOAT3(r * cosf(t1), +halfH + r * sinf(t1), 0), {0,0,0}, {1,1,1,1}, {0,0} });
				capVerts.push_back({ XMFLOAT3(r * cosf(t2), +halfH + r * sinf(t2), 0), {0,0,0}, {1,1,1,1}, {0,0} });
				// ZY arch
				capVerts.push_back({ XMFLOAT3(0, +halfH + r * sinf(t1), r * cosf(t1)), {0,0,0}, {1,1,1,1}, {0,0} });
				capVerts.push_back({ XMFLOAT3(0, +halfH + r * sinf(t2), r * cosf(t2)), {0,0,0}, {1,1,1,1}, {0,0} });
			}

			// Bottom dome arch (XY & ZY)
			for (int i = 0; i < SEGS / 2; i++) {
				float t1 = (float)i * 3.14159265f / (float)(SEGS / 2);
				float t2 = (float)(i + 1) * 3.14159265f / (float)(SEGS / 2);
				// XY arch
				capVerts.push_back({ XMFLOAT3(r * cosf(t1), -halfH - r * sinf(t1), 0), {0,0,0}, {1,1,1,1}, {0,0} });
				capVerts.push_back({ XMFLOAT3(r * cosf(t2), -halfH - r * sinf(t2), 0), {0,0,0}, {1,1,1,1}, {0,0} });
				// ZY arch
				capVerts.push_back({ XMFLOAT3(0, -halfH - r * sinf(t1), r * cosf(t1)), {0,0,0}, {1,1,1,1}, {0,0} });
				capVerts.push_back({ XMFLOAT3(0, -halfH - r * sinf(t2), r * cosf(t2)), {0,0,0}, {1,1,1,1}, {0,0} });
			}

			if (capVerts.size() <= CAPSULE_MAX_VERTS) {
				void* data = nullptr;
				if (SUCCEEDED(s_capsuleVB->Resource->Map(0, nullptr, &data))) {
					memcpy(data, capVerts.data(), sizeof(Vertex) * capVerts.size());
					s_capsuleVB->Resource->Unmap(0, nullptr);

					XMMATRIX worldMatrix = rotation * translation;
					Renderer::SetWorldMatrix(worldMatrix);
					Renderer::BindShaderConstantsDX12();

					D3D12_VERTEX_BUFFER_VIEW vbView = {};
					vbView.BufferLocation = s_capsuleVB->Resource->GetGPUVirtualAddress();
					vbView.StrideInBytes = s_capsuleVB->Stride;
					vbView.SizeInBytes = s_capsuleVB->Stride * (UINT)capVerts.size();
					cmdList->IASetVertexBuffers(0, 1, &vbView);

					cmdList->DrawInstanced((UINT)capVerts.size(), 1, 0, 0);
				}
			}
		}
	}
}

