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

void PhysicsSystem::Update() {
	PhysicsMetrics metrics;
	metrics.totalRigidBodies = (int)_rigidBodys.size();
	metrics.totalColliders = (int)_colliders.size();

	// 1. Integrate
	{
		PROFILE_SCOPE("Physics: Integrate");
		for (auto* rb : _rigidBodys) {
			if (rb->GetActive()) {
				metrics.activeRigidBodies++;
				rb->Integrate();
			}
		}
	}

	for (auto* col : _colliders) {
		if (col->GetActive()) {
			metrics.activeColliders++;
		}
	}

	if (_colliders.size() < 2) {
		Profiler::RecordPhysicsMetrics(metrics);
		return;
	}

	// 2. Broadphase & Sort
	{
		PROFILE_SCOPE("Physics: Broadphase");
		for (auto* col : _colliders) {
			col->BeginFrame();
		}

		std::sort(_colliders.begin(), _colliders.end(),
			[](Collider* a, Collider* b) {
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
				metrics.broadphasePairs++;
				auto* colA = _colliders[i];
				auto* colB = _colliders[j];

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
					else
						colA->GetOwner()->DispatchOnCollisionStay(colB);

					if (colB->_previousOverlaps.find(colA) == colB->_previousOverlaps.end()) {
						colB->GetOwner()->DispatchOnCollisionEnter(colA);
						if (colB->GetOwner()->GetComponent<RigidBody>())
							colB->GetOwner()->GetComponent<RigidBody>()->WakeUp();
					}
					else
						colB->GetOwner()->DispatchOnCollisionStay(colA);
				
					_manifolds.push_back(manifold);
				}
			}
		}

		// Exit events
		for (auto* col : _colliders) {
			for (auto* prevCol : col->_previousOverlaps) {
				if (col->_currentOverlaps.find(prevCol) == col->_currentOverlaps.end()) {
					col->GetOwner()->DispatchOnCollisionExit(prevCol);
					if(col->GetOwner()->GetComponent<RigidBody>())
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
				KTVECTOR3 rA = cp.position - manifold.a->GetOwner()->_transform._position;
				KTVECTOR3 rB = cp.position - manifold.b->GetOwner()->_transform._position;

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

		for (auto& manifold : _manifolds) {
			for (int iter = 0; iter < 20; iter++) {
				ResolveInpulse(manifold);
			}
			ResolveCollision(manifold);
		}

		for (auto* col : _colliders) {
			col->EndFrame();
		}
	}

	Profiler::RecordPhysicsMetrics(metrics);
}

void PhysicsSystem::UpdateEditor() {
	PhysicsMetrics metrics;
	metrics.totalRigidBodies = (int)_rigidBodys.size();
	metrics.totalColliders = (int)_colliders.size();

	for (auto* col : _colliders) {
		if (col && col->GetActive() && col->GetOwner() && col->GetOwner()->GetActive()) {
			metrics.activeColliders++;
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
		RigidBody* rb = col->GetOwner()->GetComponent<RigidBody>();
		if (rb == nullptr)continue;
		bool massChanged = ((rb->_oldMass - rb->_mass) * (rb->_oldMass - rb->_mass) > 1e-6f);
		if (!col->_hasChangedScale && !massChanged) continue; //スケールか質量が変化していなければスキップ
		if (rb->_mass > 0.0f && !rb->_isKinematic) {
			rb->_inertiaTensorBody = col->ComputeLocalInertiaTensor(rb->_mass);
			rb->_inertiaTensorBodyInv = rb->_inertiaTensorBody.Inverse();
		}
		else {
			rb->_inertiaTensorBody = KTMATRIX3::Zero();
			rb->_inertiaTensorBodyInv = KTMATRIX3::Zero();
		}
		rb->_oldMass = rb->_mass;
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

	for (auto& contact : manifold.contacts) {//法線方向

		KTVECTOR3 rA = contact.position - manifold.a->GetOwner()->_transform._position;
		KTVECTOR3 rB = contact.position - manifold.b->GetOwner()->_transform._position;

		//速度修正
		KTVECTOR3 vA = rbA ? rbA->_velocity + Cross(rbA->_angularVelocity, rA) : KTVECTOR3(0.0f, 0.0f, 0.0f);
		KTVECTOR3 vB = rbB ? rbB->_velocity + Cross(rbB->_angularVelocity, rB) : KTVECTOR3(0.0f, 0.0f, 0.0f);
		//相対速度
		KTVECTOR3 rV = vA - vB;//B->Aの相対速度
		float relVelAlongNormal = Dot(rV, manifold.normal);//manifold.normal => B->A方向
		// 離れていく場合はスキップ(B->A方向で一致→内積が正の値の場合離れていく) && 法線インパルスが0以下の場合スキップ
		if (relVelAlongNormal > 0.0f && contact.normalImpulseSum <= 0.0f)continue;

		//有効質量
		KTVECTOR3 rnA = rbA ? Cross(rA, manifold.normal) : KTVECTOR3(0.0f, 0.0f, 0.0f);
		KTVECTOR3 rnB = rbB ? Cross(rB, manifold.normal) : KTVECTOR3(0.0f, 0.0f, 0.0f);

		float angA = rbA ? Dot(rbA->_inertiaTensorWorldInv * rnA, rnA) : 0.0f;
		float angB = rbB ? Dot(rbB->_inertiaTensorWorldInv * rnB, rnB) : 0.0f;
		float normalMass = invMassA + invMassB + angA + angB;

		if (normalMass <= 0.0f) continue;

		// 衝突インパルスの計算（累積処理）
		//deltaImpulseの計算
		float deltaImpulse = (contact.velocityBias - relVelAlongNormal) / normalMass;


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
	if(rbA)rbA->CheckSleep();
	if(rbB)rbB->CheckSleep();
}

void PhysicsSystem::ApplyWarmStarting(){

	static std::vector<ManifoldKey> searchList;
	searchList.clear();
	searchList.reserve(_prevManifolds.size());	//前フレームのマニフォールドと同サイズ分

	//リスト作成
	for(auto& old : _prevManifolds){
		uint64_t key = MakePairKey(old.a, old.b);
		searchList.push_back({ key, &old });
	}

	//ソート
	std::sort(searchList.begin(), searchList.end());

	for (auto& newManifold : _manifolds) {
		//前フレームの同じペアのマニフォールドを探す
		uint64_t key = MakePairKey(newManifold.a, newManifold.b);

		//二分探索
		ManifoldKey target = { key, nullptr };
		auto it = std::lower_bound(searchList.begin(), searchList.end(), target);

		if (it != searchList.end() && it->key == key) {//見つかった場合
			CollisionManifold* oldManifold = it->manifold;

			//剛体の取得
			RigidBody* rbA = newManifold.a->GetOwner()->GetComponent<RigidBody>();
			RigidBody* rbB = newManifold.b->GetOwner()->GetComponent<RigidBody>();
			if (!rbA && !rbB) continue;

			//接触点のマッチング
			for (auto& newContact : newManifold.contacts) {

				float minDistSqr = FLT_MAX;
				ContactPoint* bestOldContact = nullptr;

				for (auto& oldContact : oldManifold->contacts) {
					float distSqr = (newContact.position - oldContact.position).MagnitudeSqr();

					//距離比較
					if (distSqr < 0.005f && distSqr < minDistSqr) {
						minDistSqr = distSqr;
						bestOldContact = &oldContact;
					}
				}

				//マッチする古い点が見つかったら累積インパルスを引き継ぐ
				if (bestOldContact) {
					newContact.normalImpulseSum = bestOldContact->normalImpulseSum;
					newContact.tangentImpulseSum = bestOldContact->tangentImpulseSum;

					//引き継いだインパルスを事前に速度に適用
					KTVECTOR3 rA = newContact.position - newManifold.a->GetOwner()->_transform._position;
					KTVECTOR3 rB = newContact.position - newManifold.b->GetOwner()->_transform._position;

					//法線方向のインパルス復元
					KTVECTOR3 applyImpulse = newContact.normalImpulseSum * newManifold.normal;
					//接線方向のインパルス復元
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
		XMMATRIX translation = XMMatrixTranslation(owner->_transform._position.x, owner->_transform._position.y, owner->_transform._position.z);
		XMFLOAT4 q = XMFLOAT4(owner->_transform._quaternion.x, owner->_transform._quaternion.y, owner->_transform._quaternion.z, owner->_transform._quaternion.w);
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

