//=====================================================================================
// Collider.cpp
// Author:Kaito Aoki
// Date:2025/09/08
//=====================================================================================

#include "Collider.h"
#include "Sphere.h"
#include "Capsule.h"
#include "GameObject.h"
#include "Manager.h"
#include "Scene.h"
#include "Renderer.h"
#include "Texture.h"
#include "ShaderManager.h"
#include "Shader.h"
#include "ModelRenderer.h"
#include "RigidBody.h"
#include <set>
#include <cmath>
#include <algorithm>

void ClosestPointSegSeg(const KTVECTOR3& s1, const KTVECTOR3& e1, const KTVECTOR3& s2, const KTVECTOR3& e2, KTVECTOR3& c1, KTVECTOR3& c2){
	float s, t; // パラメータ

	KTVECTOR3 d1 = e1 - s1;	// 線分1の方向ベクトル
	KTVECTOR3 d2 = e2 - s2;
	KTVECTOR3 r = s1 - s2;	// 始点間のベクトル(s2->s1)

	float lenSqD1 = d1.MagnitudeSqr(); // 線分1の長さの2乗
	float lenSqD2 = d2.MagnitudeSqr();
	float f = Dot(d2, r);

	const float EPSILON = 1e-6f;

	if (lenSqD1 <= EPSILON && lenSqD2 <= EPSILON) { // 両方とも点に退化した場合
		s = t = 0.0f;
		c1 = s1;
		c2 = s2;
		return;
	}
	if (lenSqD1 <= EPSILON) {	// 線分1が点に退化した場合
		s = 0.0f;
		t = Clamp(f / lenSqD2, 0.0f, 1.0f);
	}
	else {	// 線分1は点ではない
		float c = Dot(d1, r);
		if (lenSqD2 <= EPSILON) {	// 線分2が点に退化
			t = 0.0f;
			s = Clamp(-c / lenSqD1, 0.0f, 1.0f);
		}
		else {	// 通常の場合
			float b = Dot(d1, d2);	// 方向ベクトル同士の内積
			float denom = lenSqD1 * lenSqD2 - b * b;	// 行列式の分母

			if (denom != 0.0f) { // 平行でない場合
				s = Clamp((b * f - c * lenSqD2) / denom, 0.0f, 1.0f);
			}
			else { // 平行な場合
				s = 0.0f;
			}
			t = (b * s + f) / lenSqD2;

	// tを0~1にクランプしてsを再計算
			if (t < 0.0f) {
				t = 0.0f;
				s = Clamp(-c / lenSqD1, 0.0f, 1.0f);
			}
			else if (t > 1.0f) {
				t = 1.0f;
				s = Clamp((b - c) / lenSqD1, 0.0f, 1.0f);
			}
		}
	}
	c1 = s1 + d1 * s;
	c2 = s2 + d2 * t;
}

KTVECTOR3 Collider::GetWorldCenter() const {
	return _owner ? _owner->GetWorldPosition() : _center;
}

void ColliderSphere::Awake(){
	_executeInEditor = true;
	if (Manager::GetCurrentScene() && Manager::GetCurrentScene()->GetPhysicsSystem()) {
		Manager::GetCurrentScene()->GetPhysicsSystem()->RegisterCollider(this);
	}
}

void ColliderSphere::Start(){
	if (Manager::GetCurrentScene() && Manager::GetCurrentScene()->GetPhysicsSystem()) {
		Manager::GetCurrentScene()->GetPhysicsSystem()->RegisterCollider(this);
	}
}

void ColliderSphere::OnDestroy(){
	if (Manager::GetCurrentScene() && Manager::GetCurrentScene()->GetPhysicsSystem()) {
		Manager::GetCurrentScene()->GetPhysicsSystem()->RemoveCollider(this);
	}
}

void ColliderSphere::OnDestroyOnEditor(){
	if (Manager::GetCurrentScene() && Manager::GetCurrentScene()->GetPhysicsSystem()) {
		Manager::GetCurrentScene()->GetPhysicsSystem()->RemoveCollider(this);
	}
}

void ColliderSphere::Update(){
	if (Manager::GetCurrentScene() && Manager::GetCurrentScene()->GetPhysicsSystem()) {
		Manager::GetCurrentScene()->GetPhysicsSystem()->RegisterCollider(this);
	}
	SyncTransform();
}

void ColliderSphere::SyncTransform(){
	if (!_owner) return;
	// _owner->GetWorldScale()の最大値を反映
	float tempScale = _owner->GetWorldScale().x;
	if (tempScale < _owner->GetWorldScale().y) {
		tempScale = _owner->GetWorldScale().y;
	}
	if (tempScale < _owner->GetWorldScale().z) {
		tempScale = _owner->GetWorldScale().z;
	}
	// SphereMeshの半径を反映
	Sphere* sp;
	if (sp = _owner->GetComponent<Sphere>()) {
		_radius = sp->GetRadius() * tempScale;
	}
	else
		_radius = tempScale * 0.5f;

	// AABBの更新
	_aabb.min = _owner->GetWorldPosition() - KTVECTOR3(_radius, _radius, _radius);
	_aabb.max = _owner->GetWorldPosition() + KTVECTOR3(_radius, _radius, _radius);

	if ((_oldRadius - _radius) * (_oldRadius - _radius) > 1e-6f)
		_hasChangedScale = true;
	else
		_hasChangedScale = false;

	_oldRadius = _radius;

}

bool ColliderSphere::CheckVSSphere(const ColliderSphere* other, CollisionManifold& outCollisionManifold) const{

	if(!this->_aabb.CheckOverlap(other->_aabb))
		return false;

	outCollisionManifold.a = const_cast<ColliderSphere*>(other);
	outCollisionManifold.b = const_cast<ColliderSphere*>(this);

	// 中心間の距離の2乗
	float distanceSqr = (this->_owner->GetWorldPosition() -
		other->_owner->GetWorldPosition()).MagnitudeSqr();

	// 半径の和の2乗
	float radiusSumSqr = (this->_radius + other->_radius) * (this->_radius + other->_radius);

	if (distanceSqr <= radiusSumSqr) { // 衝突している
		outCollisionManifold.penetrationDepth = sqrtf(radiusSumSqr) - sqrtf(distanceSqr);
		outCollisionManifold.normal = (other->_owner->GetWorldPosition() -
			this->_owner->GetWorldPosition()).Normalize();
		KTVECTOR3 cpa; // Aの接触点
		cpa = other->_owner->GetWorldPosition() -
			_radius * outCollisionManifold.normal;

		KTVECTOR3 cpb; // Bの接触点
		cpb = this->_owner->GetWorldPosition() +
			_radius * outCollisionManifold.normal;

		ContactPoint cp; // 接触点情報の構築用
		cp.position = (cpa + cpb) / 2.0f;
		cp.penetration = outCollisionManifold.penetrationDepth;
		outCollisionManifold.contacts.push_back(cp);

		return true;
	}
	else
		return false;
}

bool ColliderSphere::CheckVSOBB(const ColliderBox* other, CollisionManifold& outCollisionManifold)const {

	if (!this->_aabb.CheckOverlap(other->_aabb))
		return false;

	outCollisionManifold.a = const_cast<ColliderBox*>(other);
	outCollisionManifold.b = const_cast<ColliderSphere*>(this);

	KTVECTOR3 bPos = other->GetOwner()->GetWorldPosition();	// OBBのワールド座標
	KTVECTOR3 cPos = this->_owner->GetWorldPosition();		// Sphereのワールド座標

	KTVECTOR3 BToC = cPos - bPos;
	float q[3];
	float c[3];
	for (int i = 0; i < 3; i++) {
		q[i] = Dot(BToC, other->_axis[i]);
	}
	c[0] = Clamp(q[0], -other->_extents.x, other->_extents.x);
	c[1] = Clamp(q[1], -other->_extents.y, other->_extents.y);
	c[2] = Clamp(q[2], -other->_extents.z, other->_extents.z);

	KTVECTOR3 closestPoint =
		bPos + c[0] * other->_axis[0] +
		c[1] * other->_axis[1] + c[2] * other->_axis[2];

	KTVECTOR3 diff = closestPoint - cPos;
	// 浮動小数点誤差による微小な横方向成分の切り捨て（真上からの落下時に回転を誘発するのを防止）
	if (fabsf(diff.x) < 1e-5f) diff.x = 0.0f;
	if (fabsf(diff.y) < 1e-5f) diff.y = 0.0f;
	if (fabsf(diff.z) < 1e-5f) diff.z = 0.0f;

	float distanceSqr = diff.MagnitudeSqr();
	float radiusSqr = this->_radius * this->_radius;

	if (distanceSqr <= radiusSqr) {
		if (distanceSqr > 1e-6f) {
			float dist = sqrtf(distanceSqr);
			outCollisionManifold.penetrationDepth = this->_radius - dist;
			outCollisionManifold.normal = diff / dist;
		}
		else {
			// 球の中心がOBB内部に入り込んでいる場合、最も近い面へ押し出す法線を算出
			float minDist = FLT_MAX;
			KTVECTOR3 normalLocal(0.0f, -1.0f, 0.0f);
			float dx = other->_extents.x - fabsf(q[0]);
			if (dx < minDist) { minDist = dx; normalLocal = KTVECTOR3((q[0] >= 0.0f) ? -1.0f : 1.0f, 0.0f, 0.0f); }
			float dy = other->_extents.y - fabsf(q[1]);
			if (dy < minDist) { minDist = dy; normalLocal = KTVECTOR3(0.0f, (q[1] >= 0.0f) ? -1.0f : 1.0f, 0.0f); }
			float dz = other->_extents.z - fabsf(q[2]);
			if (dz < minDist) { minDist = dz; normalLocal = KTVECTOR3(0.0f, 0.0f, (q[2] >= 0.0f) ? -1.0f : 1.0f); }

			outCollisionManifold.penetrationDepth = this->_radius + minDist;
			outCollisionManifold.normal = (
				other->_axis[0] * normalLocal.x +
				other->_axis[1] * normalLocal.y +
				other->_axis[2] * normalLocal.z).Normalize();
		}

		// 軸方向の微小な数値誤差をクリーンアップ
		if (fabsf(outCollisionManifold.normal.x) < 1e-5f) outCollisionManifold.normal.x = 0.0f;
		if (fabsf(outCollisionManifold.normal.y) < 1e-5f) outCollisionManifold.normal.y = 0.0f;
		if (fabsf(outCollisionManifold.normal.z) < 1e-5f) outCollisionManifold.normal.z = 0.0f;
		outCollisionManifold.normal = outCollisionManifold.normal.Normalize();

		ContactPoint cp;
		cp.penetration = outCollisionManifold.penetrationDepth;
		cp.position = closestPoint;
		outCollisionManifold.contacts.push_back(cp);
		return true;
	}
	return false;
}

bool ColliderSphere::CheckVSCapsule(const ColliderCapsule* other, CollisionManifold& outCollisionManifold)const {
	return other->CheckVSSphere(this, outCollisionManifold);
}

KTMATRIX3 ColliderSphere::ComputeLocalInertiaTensor(float mass){
	float v = (2.0f / 5.0f) * mass * _radius * _radius;
	return KTMATRIX3(
		v, 0.0f, 0.0f,
		0.0f, v, 0.0f,
		0.0f, 0.0f, v
	);
}

void ColliderSphere::ShowUI(){
	ImGui::Checkbox("_wasOverlap", &_wasOverlap);
}


void ColliderBox::Awake() {
	_executeInEditor = true;

	_center = _owner->GetWorldPosition();
	_axis[0] = _owner->GetRight();
	_axis[1] = _owner->GetUp();
	_axis[2] = _owner->GetForward();

	_extents = _owner->GetWorldScale() * 0.5f;

	if (Manager::GetCurrentScene() && Manager::GetCurrentScene()->GetPhysicsSystem()) {
		Manager::GetCurrentScene()->GetPhysicsSystem()->RegisterCollider(this);
	}
}

void ColliderBox::Start(){
	if (Manager::GetCurrentScene() && Manager::GetCurrentScene()->GetPhysicsSystem()) {
		Manager::GetCurrentScene()->GetPhysicsSystem()->RegisterCollider(this);
	}
}

void ColliderBox::OnDestroy(){
	if (Manager::GetCurrentScene() && Manager::GetCurrentScene()->GetPhysicsSystem()) {
		Manager::GetCurrentScene()->GetPhysicsSystem()->RemoveCollider(this);
	}
}

void ColliderBox::OnDestroyOnEditor(){
	if (Manager::GetCurrentScene() && Manager::GetCurrentScene()->GetPhysicsSystem()) {
		Manager::GetCurrentScene()->GetPhysicsSystem()->RemoveCollider(this);
	}
}

void ColliderBox::Update() {
	if (Manager::GetCurrentScene() && Manager::GetCurrentScene()->GetPhysicsSystem()) {
		Manager::GetCurrentScene()->GetPhysicsSystem()->RegisterCollider(this);
	}
	SyncTransform();
}

void ColliderBox::SyncTransform() {
	if (!_owner) return;
	// ローカル情報の更新
	_center = _owner->GetWorldPosition();
	_axis[0] = _owner->GetRight();
	_axis[1] = _owner->GetUp();
	_axis[2] = _owner->GetForward();

	_extents = _owner->GetWorldScale() * 0.5f;

	// AABBの更新
	KTVECTOR3 r;
	r.x = fabsf(_axis[0].x) * _extents.x + fabsf(_axis[1].x) * _extents.y + fabsf(_axis[2].x) * _extents.z;
	r.y = fabsf(_axis[0].y) * _extents.x + fabsf(_axis[1].y) * _extents.y + fabsf(_axis[2].y) * _extents.z;
	r.z = fabsf(_axis[0].z) * _extents.x + fabsf(_axis[1].z) * _extents.y + fabsf(_axis[2].z) * _extents.z;

	_aabb.min = _owner->GetWorldPosition() - r;
	_aabb.max = _owner->GetWorldPosition() + r;

	if ((_oldExtents - _extents).MagnitudeSqr() > 1e-6f)
		_hasChangedScale = true;
	else
		_hasChangedScale = false;

	_oldExtents = _extents;

}

bool ColliderBox::CheckVSOBB(const ColliderBox* other, CollisionManifold& manifold) const {

	if (!this->_aabb.CheckOverlap(other->_aabb))
		return false;

	manifold.a = const_cast<ColliderBox*>(other);
	manifold.b = const_cast<ColliderBox*>(this);

	float minOverlap = FLT_MAX;	// 最小重複量
	KTVECTOR3 bestAxis;			// 最小重複軸（分離軸）

	// 各軸について重複チェック
	//SAT
	for (int i = 0; i < 3; i++) {
		float overlap = 0.0f;
		if (!OverlapOnAxis(other, _axis[i], overlap)) return false;
		if (overlap < minOverlap) {
			minOverlap = overlap;
			bestAxis = _axis[i];
		}

		if (!OverlapOnAxis(other, other->_axis[i], overlap)) return false;
		if (overlap < minOverlap) {
			minOverlap = overlap;
			bestAxis = other->_axis[i];
		}
	}

	// 外積軸の重複確認
	for (int i = 0; i < 3; i++) {
		for (int j = 0; j < 3; j++) {
			KTVECTOR3 axis = Cross(_axis[i], other->_axis[j]);
			if (axis.MagnitudeSqr() < DBL_EPSILON) continue;

			float overlap = 0.0f;
			if (!OverlapOnAxis(other, axis.Normalize(), overlap)) return false;
			if(overlap < minOverlap) {
				minOverlap = overlap;
				bestAxis = axis;
			}
		}
	}

	// 衝突法線の設定

	RigidBody* rbA = other->GetOwner() ? other->GetOwner()->GetComponent<RigidBody>() : nullptr;
	RigidBody* rbB = _owner ? _owner->GetComponent<RigidBody>() : nullptr;
	KTVECTOR3 vA = (rbA && !rbA->IsSleeping() && !rbA->_isKinematic) ? rbA->_velocity : KTVECTOR3(0.0f, 0.0f, 0.0f);
	KTVECTOR3 vB = (rbB && !rbB->IsSleeping() && !rbB->_isKinematic) ? rbB->_velocity : KTVECTOR3(0.0f, 0.0f, 0.0f);
	KTVECTOR3 prevCenterDelta = (other->_center - vA * (1.0f / 60.0f)) - (_center - vB * (1.0f / 60.0f));
	KTVECTOR3 centerDelta = (prevCenterDelta.MagnitudeSqr() > 1e-6f) ? prevCenterDelta : (other->_center - _center);
	if (Dot(centerDelta, bestAxis) < 0.0f)
		bestAxis = -bestAxis;

	manifold.hasCollision = true;
	manifold.normal = bestAxis.Normalize();
	manifold.penetrationDepth = minOverlap;


	// 衝突点の計算
	FixedList<KTVECTOR3, 16> contactPolygon = ComputeContactPolygon(this, other, manifold.normal);
	if (!contactPolygon.empty()) {
		for (auto& p : contactPolygon) {
			ContactPoint cp;
			cp.position = p;
	cp.penetration = minOverlap;	// めり込み量
			manifold.contacts.push_back(cp);
		}
	}
	else { // クリップできなかった場合の最近傍点
		KTVECTOR3 pointOnA = _center + manifold.normal * (_extents.x + _extents.y + _extents.z);
		KTVECTOR3 pointOnB = other->_center - manifold.normal * (other->_extents.x + other->_extents.y + other->_extents.z);
		ContactPoint cp;
		cp.position = (pointOnA + pointOnB) * 0.5f;
		cp.penetration = minOverlap;
		manifold.contacts.push_back(cp);
	}
	
	// 全ての軸で重複しているので衝突している
	return true;
}

bool ColliderBox::CheckVSSphere(const ColliderSphere* other, CollisionManifold& outCollisionManifold) const {
	return other->CheckVSOBB(this, outCollisionManifold);
}

bool ColliderBox::CheckVSCapsule(const ColliderCapsule* other, CollisionManifold& outCollisionManifold)const {
	return other->CheckVSOBB(this, outCollisionManifold);
}


bool ColliderBox::OverlapOnAxis(const ColliderBox* other, const KTVECTOR3& axis) const{

	if (axis.Magnitude() < 1e-6f) return true;

	KTVECTOR3 L = axis.Normalize();

	// 自身の軸
	float centerA = Dot(_center, L);
	float rA = _extents.x * fabs(Dot(_axis[0], L)) +
		_extents.y * fabs(Dot(_axis[1], L)) +
		_extents.z * fabs(Dot(_axis[2], L));

	// 相手の軸
	float centerB = Dot(other->_center, L);
	float rB = other->_extents.x * fabs(Dot(other->_axis[0], L)) +
		other->_extents.y * fabs(Dot(other->_axis[1], L)) +
		other->_extents.z * fabs(Dot(other->_axis[2], L));

	//S
	float distance = fabs(centerA - centerB);

	// 外積軸
	return distance <= (rA + rB);

}

bool ColliderBox::OverlapOnAxis(const ColliderBox* other, const KTVECTOR3& axis, float& outOverlap) const
{
	if (axis.Magnitude() < 1e-6f) {
		outOverlap = FLT_MAX;
		return true;
	}

	KTVECTOR3 L = axis.Normalize();

	// 自身の軸
	float centerA = Dot(_center, L);
	float rA = _extents.x * fabs(Dot(_axis[0], L)) +
		_extents.y * fabs(Dot(_axis[1], L)) +
		_extents.z * fabs(Dot(_axis[2], L));

	// 相手の軸
	float centerB = Dot(other->_center, L);
	float rB = other->_extents.x * fabs(Dot(other->_axis[0], L)) +
		other->_extents.y * fabs(Dot(other->_axis[1], L)) +
		other->_extents.z * fabs(Dot(other->_axis[2], L));

	//S
	float distance = fabs(centerA - centerB);
	outOverlap = (rA + rB) - distance;

	// 外積軸
	return distance <= (rA + rB);
}

FixedList<KTVECTOR3, 4> ColliderBox::GetFaceVertices(const ColliderBox* box, int axisIndex, int sign){
	FixedList<KTVECTOR3, 4> verts;

	// extents の各成分
	float ex = box->_extents.x;
	float ey = box->_extents.y;
	float ez = box->_extents.z;

	float extent = (axisIndex == 0) ? ex : (axisIndex == 1 ? ey : ez);

	// face center
	KTVECTOR3 faceCenter = box->_center + box->_axis[axisIndex] * (extent * (float)sign);

	// face の頂点2つ
	int iu = (axisIndex + 1) % 3;
	int iv = (axisIndex + 2) % 3;

	float eu = (iu == 0) ? ex : (iu == 1 ? ey : ez);
	float ev = (iv == 0) ? ex : (iv == 1 ? ey : ez);

	KTVECTOR3 axisU = box->_axis[iu];
	KTVECTOR3 axisV = box->_axis[iv];

	verts.push_back(faceCenter + axisU * eu + axisV * ev);
	verts.push_back(faceCenter - axisU * eu + axisV * ev);
	verts.push_back(faceCenter - axisU * eu - axisV * ev);
	verts.push_back(faceCenter + axisU * eu - axisV * ev);

	return verts;
}

FixedList<Plane, 8> ColliderBox::GetOBBPlanes(const ColliderBox* box){
	FixedList<Plane, 8> planes;

	float ex = box->_extents.x;
	float ey = box->_extents.y;
	float ez = box->_extents.z;

	for (int i = 0; i < 3; ++i) {
		KTVECTOR3 a = box->_axis[i].Normalize();
		float extent = (i == 0) ? ex : (i == 1 ? ey : ez);

		// +
		KTVECTOR3 pPos = box->_center + a * extent;
		planes.push_back({ a, Dot(a, pPos) });

		// -
		KTVECTOR3 pNeg = box->_center - a * extent;
		planes.push_back({ -a, Dot(-a, pNeg) });
	}

	return planes;
}

FixedList<KTVECTOR3, 16> ColliderBox::ClipPolygonAgainstPlane(const FixedList<KTVECTOR3, 16>& polygon, const Plane& plane, float eps){
	FixedList<KTVECTOR3, 16> out;
	if (polygon.empty()) return out;

	auto inside = [&](const KTVECTOR3& v) {
		// : Dot(n, v) <= d (+ eps }[W)
		return Dot(plane.n, v) <= plane.d + eps;
		};

	size_t N = polygon.size();
	for (size_t i = 0; i < N; ++i) {
		const KTVECTOR3& A = polygon[i];
		const KTVECTOR3& B = polygon[(i + 1) % N];

		bool inA = inside(A);
		bool inB = inside(B);

		if (inA && inB) {
	// 頂点Bを追加
			out.push_back(B);
		}
		else if (inA && !inB) {
	// A inside, B outside -> 交点を追加
			float da = Dot(plane.n, A) - plane.d;
			float db = Dot(plane.n, B) - plane.d;
			float t = da / (da - db); // da/(da-db)
			KTVECTOR3 P = A + (B - A) * t;
			out.push_back(P);
		}
		else if (!inA && inB) {
			// A outside, B inside -> _ + B
			float da = Dot(plane.n, A) - plane.d;
			float db = Dot(plane.n, B) - plane.d;
			float t = da / (da - db);
			KTVECTOR3 P = A + (B - A) * t;
			out.push_back(P);
			out.push_back(B);
		}
		else {
	// 出力頂点リストへコピー
		}
	}

	return out;
}


FixedList<KTVECTOR3, 16> ColliderBox::ComputeContactPolygon(const ColliderBox* refBox, const ColliderBox* incBox, const KTVECTOR3& collisionNormal){
	// 基準ボックス(refBox)と基準面の特定
	// 基準面: bestAxis に最も近い面 (abs dot が最大)
	int refAxis = 0;
	float bestDot = fabs(Dot(refBox->_axis[0], collisionNormal));
	for (int i = 1; i < 3; ++i) {
		float d = fabs(Dot(refBox->_axis[i], collisionNormal));
		if (d > bestDot) { bestDot = d; refAxis = i; }
	}

	// 基準面の法線
	float s = Dot(collisionNormal, refBox->_axis[refAxis]);
	int refSign = (s >= 0.0f) ? +1 : -1;

	// 基準面の頂点
	FixedList<KTVECTOR3, 16> poly;
	auto initalVerts =  GetFaceVertices(refBox, refAxis, refSign);

	// 計算用の一時配列にコピー
	for(const auto& v : initalVerts){
		poly.push_back(v);
	}

	// 入射ボックス(incBox)の面を取得
	FixedList<Plane, 8> incPlanes = GetOBBPlanes(incBox);

	// 多角形を基準面の各面でクリップ
	for (const Plane& pl : incPlanes) {
		poly = ClipPolygonAgainstPlane(poly, pl);
		if (poly.empty()) break;
	}

	return poly; // クリップ後の多角形を返す
}

KTMATRIX3 ColliderBox::ComputeLocalInertiaTensor(float mass)
{
	float ix = (1.0f / 12.0f) * mass * (_extents.y * _extents.y + _extents.z * _extents.z) * 4.0f;
	float iy = (1.0f / 12.0f) * mass * (_extents.x * _extents.x + _extents.z * _extents.z) * 4.0f;
	float iz = (1.0f / 12.0f) * mass * (_extents.x * _extents.x + _extents.y * _extents.y) * 4.0f;
	return KTMATRIX3(
		ix, 0.0f, 0.0f,
		0.0f, iy, 0.0f,
		0.0f, 0.0f, iz
	);
}

void ColliderBox::ShowUI() {
	ImGui::Checkbox("_wasOverlap", &_wasOverlap);
}

void ColliderCapsule::Awake() {
	_executeInEditor = true;
	if (Manager::GetCurrentScene() && Manager::GetCurrentScene()->GetPhysicsSystem()) {
		Manager::GetCurrentScene()->GetPhysicsSystem()->RegisterCollider(this);
	}
}

void ColliderCapsule::Start() {
	if (Manager::GetCurrentScene() && Manager::GetCurrentScene()->GetPhysicsSystem()) {
		Manager::GetCurrentScene()->GetPhysicsSystem()->RegisterCollider(this);
	}
}

void ColliderCapsule::OnDestroy() {
	if (Manager::GetCurrentScene() && Manager::GetCurrentScene()->GetPhysicsSystem()) {
		Manager::GetCurrentScene()->GetPhysicsSystem()->RemoveCollider(this);
	}
}

void ColliderCapsule::OnDestroyOnEditor() {
	if (Manager::GetCurrentScene() && Manager::GetCurrentScene()->GetPhysicsSystem()) {
		Manager::GetCurrentScene()->GetPhysicsSystem()->RemoveCollider(this);
	}
}

void ColliderCapsule::Update() {
	if (Manager::GetCurrentScene() && Manager::GetCurrentScene()->GetPhysicsSystem()) {
		Manager::GetCurrentScene()->GetPhysicsSystem()->RegisterCollider(this);
	}
	SyncTransform();
}

void ColliderCapsule::SyncTransform() {
	if (!_owner) return;
	// GameObjectのTransform更新
	// _owner->GetWorldScale() の一番大きい値を反映（均等スケール）
	float tempScaleRad = _owner->GetWorldScale().x;
	float tempScaleHeight = _owner->GetWorldScale().y;
	if (tempScaleRad < _owner->GetWorldScale().z) tempScaleRad = _owner->GetWorldScale().z;

	// 見た目（Capsuleコンポーネント）パラメータを反映
	Capsule* cap = _owner->GetComponent<Capsule>();
	if (cap) {
		_radius = cap->_radius * tempScaleRad;
		_height = cap->_height * tempScaleHeight;
	}
	else {
		_radius = tempScaleRad * 0.5f;
		_height = tempScaleHeight * 2.0f;
	}

	// AABBの更新
	float halfCylHeight = (std::max)(0.0f, _height - 2.0f * _radius) * 0.5f;

	// カプセルの軸（Upベクトル）取得
	KTVECTOR3 up = _owner->GetUp();

	// 各軸(X, Y, Z)の射影長さを計算し半径を加算
	KTVECTOR3 extents;
	extents.x = std::abs(up.x) * halfCylHeight + _radius;
	extents.y = std::abs(up.y) * halfCylHeight + _radius;
	extents.z = std::abs(up.z) * halfCylHeight + _radius;

	// AABBの最小値と最大値を更新
	_aabb.min = _owner->GetWorldPosition() - extents;
	_aabb.max = _owner->GetWorldPosition() + extents;

	if ((_oldRadius - _radius) * (_oldRadius - _radius) > 1e-6f || (_oldHeight - _height) * (_oldHeight - _height) > 1e-6f)
		_hasChangedScale = true;
	else
		_hasChangedScale = false;

	_oldRadius = _radius;
	_oldHeight = _height;
}

bool ColliderCapsule::CheckVSCapsule(const ColliderCapsule* other, CollisionManifold& outCollisionManifold) const{
	
	float cylinderHeightA = (std::max)(0.0f, this->_height - 2.0f * this->_radius);
	float cylinderHeightB = (std::max)(0.0f, other->_height - 2.0f * other->_radius);
	
	GameObject* capsuleA = this->GetOwner();
	GameObject* capsuleB = other->GetOwner();

	KTVECTOR3 upA = capsuleA->GetUp();
	KTVECTOR3 upB = capsuleB->GetUp();

	KTVECTOR3 s1 = capsuleA->GetWorldPosition() - upA * (cylinderHeightA * 0.5f);
	KTVECTOR3 e1 = capsuleA->GetWorldPosition() + upA * (cylinderHeightA * 0.5f);

	KTVECTOR3 s2 = capsuleB->GetWorldPosition() - upB * (cylinderHeightB * 0.5f);
	KTVECTOR3 e2 = capsuleB->GetWorldPosition() + upB * (cylinderHeightB * 0.5f);

	// 線分間の最近接点2点を求める
	KTVECTOR3 closestPointA, closestPointB;
	ClosestPointSegSeg(s1, e1, s2, e2, closestPointA, closestPointB);

	KTVECTOR3 diff = closestPointB - closestPointA;
	float distSq = diff.MagnitudeSqr();
	float radiusSum = this->_radius + other->_radius;

	if (distSq < radiusSum * radiusSum) { // 衝突している
		outCollisionManifold.hasCollision = true;
		outCollisionManifold.a = const_cast<ColliderCapsule*>(other);
		outCollisionManifold.b = const_cast<ColliderCapsule*>(this);

		float dist = diff.Magnitude();
		outCollisionManifold.penetrationDepth = radiusSum - dist;
		if (dist > 1e-5f)
		{
			outCollisionManifold.normal = diff.Normalize();
		}
	else { // 完全に重なっている場合
			outCollisionManifold.normal = KTVECTOR3(0.0f, 1.0f, 0.0f);
		}


		ContactPoint cp;
		cp.position = closestPointB + outCollisionManifold.normal * other->_radius;
		cp.penetration = outCollisionManifold.penetrationDepth;
		outCollisionManifold.contacts.push_back(cp);
		return true;
	}

	return false;
}

bool ColliderCapsule::CheckVSSphere(const ColliderSphere* other, CollisionManifold& outCollisionManifold)const {
	// カプセルの始点(A)と終点(B)を求める
	float cylinderHeight = (std::max)(0.0f, this->_height - 2.0f * this->_radius);
	GameObject* capsuleObj = this->GetOwner();
	KTVECTOR3 up = capsuleObj->GetUp();

	KTVECTOR3 A = capsuleObj->GetWorldPosition() - up * (cylinderHeight * 0.5f);
	KTVECTOR3 B = capsuleObj->GetWorldPosition() + up * (cylinderHeight * 0.5f);

	// 球の中心(C)
	KTVECTOR3 C = other->GetOwner()->GetWorldPosition();

	// 点Cから線分ABへの最近接点(P)を計算
	KTVECTOR3 AB = B - A;
	KTVECTOR3 AC = C - A;

	float t = 0.0f;
	float abLengthSqr = AB.MagnitudeSqr();

	// 0除算防止チェック
	if (abLengthSqr > 1e-6f) {
		t = Dot(AC, AB) / abLengthSqr;
	}

	// tを0.0〜1.0にクランプ
	t = Clamp(t, 0.0f, 1.0f);

	// 最近接点P
	KTVECTOR3 P = A + AB * t;

	// 最近接点Pと球の中心Cの衝突判定
	KTVECTOR3 diff = C - P; // PCﾖのベNg
	float distSq = diff.MagnitudeSqr();
	float radiusSum = this->_radius + other->_radius;

	if (distSq <= radiusSum * radiusSum) { // 衝突している
		outCollisionManifold.hasCollision = true;

		outCollisionManifold.a = const_cast<ColliderSphere*>(other);
		outCollisionManifold.b = const_cast<ColliderCapsule*>(this);

		float dist = diff.Magnitude();
		outCollisionManifold.penetrationDepth = radiusSum - dist;

		// 法線の計算 (カプセルから球へ向かうベクトル)
		if (dist > 1e-5f) {
			outCollisionManifold.normal = diff.Normalize(); // K
		}
		else {
		// 完全に中心が重なっている場合は適当な法線
			outCollisionManifold.normal = KTVECTOR3(0.0f, 1.0f, 0.0f);
		}

		// 接触点の計算 (球の表面または中間)
		ContactPoint cp;
		cp.position = C - outCollisionManifold.normal * other->_radius;
		cp.penetration = outCollisionManifold.penetrationDepth;
		outCollisionManifold.contacts.push_back(cp);

		return true;
	}

	return false;
}

bool ColliderCapsule::CheckVSOBB(const ColliderBox* other, CollisionManifold& outCollisionManifold)const {
	// カプセルの始点(A)と終点(B)をワールド空間で計算
	float cylinderHeight = (std::max)(0.0f, this->_height - 2.0f * this->_radius);
	KTVECTOR3 up = this->GetOwner()->GetUp();
	KTVECTOR3 A = this->GetOwner()->GetWorldPosition() - up * (cylinderHeight * 0.5f);
	KTVECTOR3 B = this->GetOwner()->GetWorldPosition() + up * (cylinderHeight * 0.5f);

	// OBBのローカル空間（AABBとして扱う）へ線分ABを変換
	KTVECTOR3 boxPos = other->GetOwner()->GetWorldPosition();
	KTVECTOR3 dA = A - boxPos;
	KTVECTOR3 dB = B - boxPos;

	// Boxのローカル空間で軸平行（回転の逆変換）
	KTVECTOR3 localA(
		Dot(dA, other->_axis[0]),
		Dot(dA, other->_axis[1]),
		Dot(dA, other->_axis[2])
	);
	KTVECTOR3 localB(
		Dot(dB, other->_axis[0]),
		Dot(dB, other->_axis[1]),
		Dot(dB, other->_axis[2])
	);

	// 交互射影法 (Alternating Projection) で最近接点を探索
	KTVECTOR3 P = (localA + localB) * 0.5f; // 線分の点P (初期値は中点)
	KTVECTOR3 Q;							// OBB上の点Q
	KTVECTOR3 ab = localB - localA;
	float abLenSq = ab.MagnitudeSqr();

	// 3回程度の反復で十分な精度に収束
	for (int i = 0; i < 3; ++i) {
		// 手順1: 点PをAABBにクランプして点Qを求める
		Q.x = Clamp(P.x, -other->_extents.x, other->_extents.x);
		Q.y = Clamp(P.y, -other->_extents.y, other->_extents.y);
		Q.z = Clamp(P.z, -other->_extents.z, other->_extents.z);

		// 手順2: 点Qから線分ABへの最近接点を求めて点Pを更新
		if (abLenSq > 1e-6f) {
			float t = Dot(Q - localA, ab) / abLenSq;
			t = Clamp(t, 0.0f, 1.0f);
			P = localA + ab * t;
		}
		else {
			P = localA;
		}
	}

	// 最終判定
	Q.x = Clamp(P.x, -other->_extents.x, other->_extents.x);
	Q.y = Clamp(P.y, -other->_extents.y, other->_extents.y);
	Q.z = Clamp(P.z, -other->_extents.z, other->_extents.z);

	// 衝突マニフォールドの構築
	KTVECTOR3 PQ = Q - P; // P(Capsule)からQ(Box)へのベクトル
	float distSq = PQ.MagnitudeSqr();

	if (distSq <= this->_radius * this->_radius) {
		outCollisionManifold.hasCollision = true;
		outCollisionManifold.a = const_cast<ColliderBox*>(other);
		outCollisionManifold.b = const_cast<ColliderCapsule*>(this);

		KTVECTOR3 normalLocal;
		float dist = 0.0f;

		if (distSq > 1e-6f) {
		// 通常接触
			dist = sqrtf(distSq);
			normalLocal = PQ / dist; // K
		}
		else {
		// カプセル中心がBox内部にめり込んでいる場合 (PQがほぼゼロ)
		// 最も近いBoxの面を見つけて、外側へ押し出す
			float minDist = FLT_MAX;

			float dx = other->_extents.x - fabs(P.x);
			if (dx < minDist) { minDist = dx; normalLocal = KTVECTOR3((P.x > 0) ? 1 : -1, 0, 0); }

			float dy = other->_extents.y - fabs(P.y);
			if (dy < minDist) { minDist = dy; normalLocal = KTVECTOR3(0, (P.y > 0) ? 1 : -1, 0); }

			float dz = other->_extents.z - fabs(P.z);
			if (dz < minDist) { minDist = dz; normalLocal = KTVECTOR3(0, 0, (P.z > 0) ? 1 : -1); }

		dist = -minDist; // めり込んでいるので符号を反転
		}

		outCollisionManifold.penetrationDepth = this->_radius - dist;

		// ローカルからワールド空間の法線に変換
		outCollisionManifold.normal =
			other->_axis[0] * normalLocal.x +
			other->_axis[1] * normalLocal.y +
			other->_axis[2] * normalLocal.z;

		// 接触点の計算 (ローカルからワールド空間に戻す)
		KTVECTOR3 Q_world = boxPos +
			other->_axis[0] * Q.x +
			other->_axis[1] * Q.y +
			other->_axis[2] * Q.z;

		ContactPoint cp;
		cp.position = Q_world; // Boxの表面を接触点とする
		cp.penetration = outCollisionManifold.penetrationDepth;
		outCollisionManifold.contacts.push_back(cp);

		return true;
	}

	return false;
}

KTMATRIX3 ColliderCapsule::ComputeLocalInertiaTensor(float mass){
	// 慣性テンソルの計算
	float cylinderHeight = (std::max)(0.0f, _height - 2.0f * _radius);

	// 半径または高さが無効な場合はゼロ行列
	if (_radius <= 0.001f || mass <= 0.0f) {
		return KTMATRIX3::Zero();
	}

	const float PI = 3.14159265359f;

	// 慣性モーメントの計算
	float volumeCylinder = PI * _radius * _radius * cylinderHeight;
	float volumeSphere = (4.0f / 3.0f) * PI * _radius * _radius * _radius;
	float volumeTotal = volumeCylinder + volumeSphere;

	if (volumeTotal == 0.0f) return KTMATRIX3::Zero();

	// 質量の分布
	float massCylinder = mass * (volumeCylinder / volumeTotal);
	float massSphere = mass * (volumeSphere / volumeTotal);

	// Y軸（中心軸）回りの慣性モーメント
	float iy = (0.5f * massCylinder * _radius * _radius) +
		(0.4f * massSphere * _radius * _radius);

	// X軸・Z軸回りの慣性モーメント
	//~ X/Z 
	float ixzCylinder = (1.0f / 12.0f) * massCylinder * (3.0f * _radius * _radius + cylinderHeight * cylinderHeight);

	// 平行軸の定理適用済み
	float ixzSphere = massSphere * (0.4f * _radius * _radius +
		0.5f * cylinderHeight * cylinderHeight +
		0.375f * cylinderHeight * _radius);

	float ix = ixzCylinder + ixzSphere;
	float iz = ix;

	// テンソル行列として返す
	return KTMATRIX3(
		ix, 0.0f, 0.0f,
		0.0f, iy, 0.0f,
		0.0f, 0.0f, iz
	);
}

void ColliderCapsule::ShowUI(){
	ImGui::Checkbox("_wasOverlap", &_wasOverlap);
}


//=====================================================================================
// ColliderMesh 実装 (3D Convex Hull 最外殻メッシュ生成・衝突判定・ワイヤーフレーム構築)
//=====================================================================================

bool ColliderSphere::CollideWith(ColliderMesh* other, CollisionManifold& outCollisionManifold) {
	return other->CheckVSSphere(this, outCollisionManifold);
}

bool ColliderBox::CollideWith(ColliderMesh* other, CollisionManifold& outCollisionManifold) {
	return other->CheckVSOBB(this, outCollisionManifold);
}

bool ColliderCapsule::CollideWith(ColliderMesh* other, CollisionManifold& outCollisionManifold) {
	return other->CheckVSCapsule(this, outCollisionManifold);
}

namespace {
	// 点Pから三角形ABC上の最近接点を求める (Ericson, Real-Time Collision Detection)
	KTVECTOR3 ClosestPointOnTriangle(const KTVECTOR3& p, const KTVECTOR3& a, const KTVECTOR3& b, const KTVECTOR3& c) {
		KTVECTOR3 ab = b - a;
		KTVECTOR3 ac = c - a;
		KTVECTOR3 ap = p - a;
		float d1 = Dot(ab, ap);
		float d2 = Dot(ac, ap);
		if (d1 <= 0.0f && d2 <= 0.0f) return a;

		KTVECTOR3 bp = p - b;
		float d3 = Dot(ab, bp);
		float d4 = Dot(ac, bp);
		if (d3 >= 0.0f && d4 <= d3) return b;

		float vc = d1 * d4 - d3 * d2;
		if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f) {
			float v = d1 / (d1 - d3);
			return a + ab * v;
		}

		KTVECTOR3 cp = p - c;
		float d5 = Dot(ab, cp);
		float d6 = Dot(ac, cp);
		if (d6 >= 0.0f && d5 <= d6) return c;

		float vb = d5 * d2 - d1 * d6;
		if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f) {
			float w = d2 / (d2 - d6);
			return a + ac * w;
		}

		float va = d3 * d6 - d5 * d4;
		if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f) {
			float w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
			return b + (c - b) * w;
		}

		float denom = 1.0f / (va + vb + vc);
		float v = vb * denom;
		float w = vc * denom;
		return a + ab * v + ac * w;
	}

	// ほぼ平行でない場合のみ方向リストに追加するヘルパー
	void AddUniqueDirection(std::vector<KTVECTOR3>& list, const KTVECTOR3& dir, float cosThreshold = 0.992f) {
		float lenSq = dir.MagnitudeSqr();
		if (lenSq < 1e-6f) return;
		KTVECTOR3 n = dir / sqrtf(lenSq);
		for (const auto& existing : list) {
			if (fabsf(Dot(existing, n)) >= cosThreshold) {
				return;
			}
		}
		list.push_back(n);
	}
}

void ColliderMesh::Awake() {
	_executeInEditor = true;
	if (Manager::GetCurrentScene() && Manager::GetCurrentScene()->GetPhysicsSystem()) {
		Manager::GetCurrentScene()->GetPhysicsSystem()->RegisterCollider(this);
	}
	RebuildMesh();
	SyncTransform();
}

void ColliderMesh::Start() {
	if (Manager::GetCurrentScene() && Manager::GetCurrentScene()->GetPhysicsSystem()) {
		Manager::GetCurrentScene()->GetPhysicsSystem()->RegisterCollider(this);
	}
	if (_localVertices.empty()) {
		RebuildMesh();
	}
	SyncTransform();
}

void ColliderMesh::OnDestroy() {
	if (Manager::GetCurrentScene() && Manager::GetCurrentScene()->GetPhysicsSystem()) {
		Manager::GetCurrentScene()->GetPhysicsSystem()->RemoveCollider(this);
	}
}

void ColliderMesh::OnDestroyOnEditor() {
	if (Manager::GetCurrentScene() && Manager::GetCurrentScene()->GetPhysicsSystem()) {
		Manager::GetCurrentScene()->GetPhysicsSystem()->RemoveCollider(this);
	}
}

void ColliderMesh::Update() {
	if (Manager::GetCurrentScene() && Manager::GetCurrentScene()->GetPhysicsSystem()) {
		Manager::GetCurrentScene()->GetPhysicsSystem()->RegisterCollider(this);
	}
	SyncTransform();
}

void ColliderMesh::RebuildMesh() {
	const MODEL* model = nullptr;
	if (_owner) {
		ModelRenderer* mr = _owner->GetComponent<ModelRenderer>();
		if (mr) {
			model = mr->GetModel();
		}
	}
	_cachedModel = model;

	std::vector<KTVECTOR3> rawPoints;
	if (model && !model->BaseVertices.empty()) {
		rawPoints.reserve(model->BaseVertices.size());
		for (const auto& v : model->BaseVertices) {
			rawPoints.push_back(KTVECTOR3(v.position.x, v.position.y, v.position.z));
		}
	} else {
		// ModelRendererが未設定の場合は単位立方体の頂点をフォールバックとして使用
		const float h = 0.5f;
		rawPoints = {
			{ -h, -h, -h }, {  h, -h, -h }, {  h,  h, -h }, { -h,  h, -h },
			{ -h, -h,  h }, {  h, -h,  h }, {  h,  h,  h }, { -h,  h,  h }
		};
	}

	BuildConvexHullFromPoints(rawPoints);
	BuildWireframeBuffers();
	_hasChangedScale = true;
}

void ColliderMesh::BuildConvexHullFromPoints(const std::vector<KTVECTOR3>& rawPoints) {
	_localVertices.clear();
	_faces.clear();
	_uniqueEdges.clear();
	_uniqueFaceNormalsLocal.clear();
	_uniqueEdgeDirsLocal.clear();

	if (rawPoints.empty()) return;

	// 1. 球面上の多方向への極値頂点抽出（最も外側にある外形頂点のみを高速に選抜）
	std::vector<KTVECTOR3> sampleDirs;
	sampleDirs.reserve(72);
	// 軸方向・対角方向 (26方向)
	for (int dx = -1; dx <= 1; ++dx) {
		for (int dy = -1; dy <= 1; ++dy) {
			for (int dz = -1; dz <= 1; ++dz) {
				if (dx == 0 && dy == 0 && dz == 0) continue;
				sampleDirs.push_back(KTVECTOR3((float)dx, (float)dy, (float)dz).Normalize());
			}
		}
	}
	// フィボナッチ球面上の均等方向 (42方向)
	const int fibCount = 42;
	const float goldenRatio = (1.0f + sqrtf(5.0f)) * 0.5f;
	for (int i = 0; i < fibCount; ++i) {
		float theta = 2.0f * 3.1415926535f * (float)i / goldenRatio;
		float phi = acosf(1.0f - 2.0f * ((float)i + 0.5f) / (float)fibCount);
		sampleDirs.push_back(KTVECTOR3(
			cosf(theta) * sinf(phi),
			cosf(phi),
			sinf(theta) * sinf(phi)
		));
	}

	std::vector<KTVECTOR3> pts;
	pts.reserve(sampleDirs.size());

	KTVECTOR3 minB = rawPoints[0];
	KTVECTOR3 maxB = rawPoints[0];
	for (const auto& p : rawPoints) {
		if (p.x < minB.x) minB.x = p.x;
		if (p.y < minB.y) minB.y = p.y;
		if (p.z < minB.z) minB.z = p.z;
		if (p.x > maxB.x) maxB.x = p.x;
		if (p.y > maxB.y) maxB.y = p.y;
		if (p.z > maxB.z) maxB.z = p.z;
	}
	KTVECTOR3 span = maxB - minB;
	float diag = span.Magnitude();
	float mergeEpsSq = (std::max)(1e-8f, (diag * 0.005f) * (diag * 0.005f));

	for (const auto& d : sampleDirs) {
		float bestProj = -FLT_MAX;
		KTVECTOR3 bestPt = rawPoints[0];
		for (const auto& p : rawPoints) {
			float proj = Dot(p, d);
			if (proj > bestProj) {
				bestProj = proj;
				bestPt = p;
			}
		}
		bool duplicate = false;
		for (const auto& existing : pts) {
			if ((existing - bestPt).MagnitudeSqr() <= mergeEpsSq) {
				duplicate = true;
				break;
			}
		}
		if (!duplicate) {
			pts.push_back(bestPt);
		}
	}

	// 2. 初期四面体 (Initial Tetrahedron) の構築
	int i0 = 0, i1 = 0;
	float maxDistSq = -1.0f;
	for (int i = 0; i < (int)pts.size(); ++i) {
		for (int j = i + 1; j < (int)pts.size(); ++j) {
			float dSq = (pts[j] - pts[i]).MagnitudeSqr();
			if (dSq > maxDistSq) {
				maxDistSq = dSq;
				i0 = i;
				i1 = j;
			}
		}
	}

	int i2 = -1;
	float maxLineDistSq = -1.0f;
	KTVECTOR3 lineDir = (pts[i1] - pts[i0]).Normalize();
	for (int i = 0; i < (int)pts.size(); ++i) {
		if (i == i0 || i == i1) continue;
		KTVECTOR3 v = pts[i] - pts[i0];
		KTVECTOR3 perp = v - lineDir * Dot(v, lineDir);
		float dSq = perp.MagnitudeSqr();
		if (dSq > maxLineDistSq) {
			maxLineDistSq = dSq;
			i2 = i;
		}
	}

	int i3 = -1;
	float maxPlaneDist = -1.0f;
	if (i2 >= 0) {
		KTVECTOR3 baseN = Cross(pts[i1] - pts[i0], pts[i2] - pts[i0]).Normalize();
		for (int i = 0; i < (int)pts.size(); ++i) {
			if (i == i0 || i == i1 || i == i2) continue;
			float d = fabsf(Dot(pts[i] - pts[i0], baseN));
			if (d > maxPlaneDist) {
				maxPlaneDist = d;
				i3 = i;
			}
		}
	}

	// 平面等の退化モデルの場合はローカルAABBから直方体凸包を生成
	if (pts.size() < 4 || i2 < 0 || i3 < 0 || maxPlaneDist < 1e-4f) {
		KTVECTOR3 c = (minB + maxB) * 0.5f;
		KTVECTOR3 e = (maxB - minB) * 0.5f;
		if (e.x < 0.02f) e.x = 0.02f;
		if (e.y < 0.02f) e.y = 0.02f;
		if (e.z < 0.02f) e.z = 0.02f;
		pts = {
			{ c.x - e.x, c.y - e.y, c.z - e.z },
			{ c.x + e.x, c.y - e.y, c.z - e.z },
			{ c.x + e.x, c.y + e.y, c.z - e.z },
			{ c.x - e.x, c.y + e.y, c.z - e.z },
			{ c.x - e.x, c.y - e.y, c.z + e.z },
			{ c.x + e.x, c.y - e.y, c.z + e.z },
			{ c.x + e.x, c.y + e.y, c.z + e.z },
			{ c.x - e.x, c.y + e.y, c.z + e.z }
		};
		i0 = 0; i1 = 1; i2 = 2; i3 = 4;
	}

	// 3. 3D Quickhull アルゴリズムによる最外殻メッシュ構築
	struct QHFace {
		int v[3];
		KTVECTOR3 normal;
		bool active = true;
	};

	KTVECTOR3 interiorCenter = (pts[i0] + pts[i1] + pts[i2] + pts[i3]) * 0.25f;

	auto makeFace = [&](int a, int b, int c) -> QHFace {
		KTVECTOR3 n = Cross(pts[b] - pts[a], pts[c] - pts[a]);
		if (Dot(n, pts[a] - interiorCenter) < 0.0f) {
			std::swap(b, c);
			n = -n;
		}
		return QHFace{ { a, b, c }, n.Normalize(), true };
	};

	std::vector<QHFace> qhFaces;
	qhFaces.push_back(makeFace(i0, i1, i2));
	qhFaces.push_back(makeFace(i0, i2, i3));
	qhFaces.push_back(makeFace(i0, i3, i1));
	qhFaces.push_back(makeFace(i1, i3, i2));

	std::vector<bool> usedPt(pts.size(), false);
	usedPt[i0] = usedPt[i1] = usedPt[i2] = usedPt[i3] = true;

	const float hullEps = (std::max)(1e-4f, diag * 0.003f);

	for (int iter = 0; iter < 64; ++iter) {
		int bestPtIdx = -1;
		float farthestDist = hullEps;

		for (size_t fIdx = 0; fIdx < qhFaces.size(); ++fIdx) {
			const auto& f = qhFaces[fIdx];
			if (!f.active) continue;
			for (int p = 0; p < (int)pts.size(); ++p) {
				if (usedPt[p]) continue;
				float dist = Dot(f.normal, pts[p] - pts[f.v[0]]);
				if (dist > farthestDist) {
					farthestDist = dist;
					bestPtIdx = p;
				}
			}
		}

		if (bestPtIdx < 0) break;
		usedPt[bestPtIdx] = true;

		// bestPtIdx から見える面を特定
		std::vector<bool> visible(qhFaces.size(), false);
		int visibleCount = 0;
		for (size_t fIdx = 0; fIdx < qhFaces.size(); ++fIdx) {
			if (!qhFaces[fIdx].active) continue;
			if (Dot(qhFaces[fIdx].normal, pts[bestPtIdx] - pts[qhFaces[fIdx].v[0]]) > 1e-5f) {
				visible[fIdx] = true;
				visibleCount++;
			}
		}
		if (visibleCount == 0) continue;

		// 地平線エッジ（Horizon Edges: 可視面と非可視面の境界エッジ）を抽出
		struct DirectedEdge { int u, v; };
		std::vector<DirectedEdge> visibleEdges;
		for (size_t fIdx = 0; fIdx < qhFaces.size(); ++fIdx) {
			if (!visible[fIdx]) continue;
			const auto& f = qhFaces[fIdx];
			visibleEdges.push_back({ f.v[0], f.v[1] });
			visibleEdges.push_back({ f.v[1], f.v[2] });
			visibleEdges.push_back({ f.v[2], f.v[0] });
		}

		std::vector<DirectedEdge> horizonEdges;
		for (const auto& e : visibleEdges) {
			bool hasOppositeInVisible = false;
			for (const auto& o : visibleEdges) {
				if (o.u == e.v && o.v == e.u) {
					hasOppositeInVisible = true;
					break;
				}
			}
			if (!hasOppositeInVisible) {
				horizonEdges.push_back(e);
			}
		}

		if (horizonEdges.empty()) continue;

		for (size_t fIdx = 0; fIdx < qhFaces.size(); ++fIdx) {
			if (visible[fIdx]) {
				qhFaces[fIdx].active = false;
			}
		}

		for (const auto& he : horizonEdges) {
			qhFaces.push_back(makeFace(he.u, he.v, bestPtIdx));
		}
	}

	// 4. 有効な三角形面と使用頂点の再インデックス化
	std::vector<int> oldToNew(pts.size(), -1);
	for (const auto& f : qhFaces) {
		if (!f.active) continue;
		for (int k = 0; k < 3; ++k) {
			int oldIdx = f.v[k];
			if (oldToNew[oldIdx] < 0) {
				oldToNew[oldIdx] = (int)_localVertices.size();
				_localVertices.push_back(pts[oldIdx]);
			}
		}
	}

	std::set<std::pair<int, int>> edgeSet;
	for (const auto& f : qhFaces) {
		if (!f.active) continue;
		int a = oldToNew[f.v[0]];
		int b = oldToNew[f.v[1]];
		int c = oldToNew[f.v[2]];
		if (a == b || b == c || c == a) continue;

		HullFace hf;
		hf.v0 = a;
		hf.v1 = b;
		hf.v2 = c;
		hf.localNormal = f.normal;
		_faces.push_back(hf);

		AddUniqueDirection(_uniqueFaceNormalsLocal, f.normal);

		int tri[3] = { a, b, c };
		for (int k = 0; k < 3; ++k) {
			int u = tri[k];
			int v = tri[(k + 1) % 3];
			if (u > v) std::swap(u, v);
			edgeSet.insert({ u, v });
		}
	}

	_uniqueEdges.assign(edgeSet.begin(), edgeSet.end());
	for (const auto& e : _uniqueEdges) {
		AddUniqueDirection(_uniqueEdgeDirsLocal, _localVertices[e.second] - _localVertices[e.first]);
		if (_uniqueEdgeDirsLocal.size() >= 16) break; // SAT高速化のため主要16方向までに制限
	}

	// ローカル中心とローカルハーフサイズを算出
	if (!_localVertices.empty()) {
		KTVECTOR3 lMin = _localVertices[0];
		KTVECTOR3 lMax = _localVertices[0];
		for (const auto& v : _localVertices) {
			if (v.x < lMin.x) lMin.x = v.x;
			if (v.y < lMin.y) lMin.y = v.y;
			if (v.z < lMin.z) lMin.z = v.z;
			if (v.x > lMax.x) lMax.x = v.x;
			if (v.y > lMax.y) lMax.y = v.y;
			if (v.z > lMax.z) lMax.z = v.z;
		}
		_localCenter = (lMin + lMax) * 0.5f;
		_localHalfExtents = (lMax - lMin) * 0.5f;
	}
}

void ColliderMesh::BuildWireframeBuffers() {
	_wireVB.reset();
	_wireIB.reset();
	_wireIndexCount = 0;

	if (_localVertices.empty() || _uniqueEdges.empty() || !Renderer::GetDeviceDX12()) {
		return;
	}

	std::vector<Vertex> gpuVerts(_localVertices.size());
	for (size_t i = 0; i < _localVertices.size(); ++i) {
		gpuVerts[i].position = XMFLOAT3(_localVertices[i].x, _localVertices[i].y, _localVertices[i].z);
		gpuVerts[i].normal = XMFLOAT3(0.0f, 1.0f, 0.0f);
		gpuVerts[i].color = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		gpuVerts[i].uv = XMFLOAT2(0.0f, 0.0f);
	}

	std::vector<unsigned int> gpuIndices;
	gpuIndices.reserve(_uniqueEdges.size() * 2);
	for (const auto& edge : _uniqueEdges) {
		gpuIndices.push_back((unsigned int)edge.first);
		gpuIndices.push_back((unsigned int)edge.second);
	}
	_wireIndexCount = (unsigned int)gpuIndices.size();

	_wireVB = Renderer::CreateVertexBuffer(sizeof(Vertex), (unsigned int)gpuVerts.size());
	if (_wireVB && _wireVB->Resource) {
		void* data = nullptr;
		if (SUCCEEDED(_wireVB->Resource->Map(0, nullptr, &data))) {
			memcpy(data, gpuVerts.data(), sizeof(Vertex) * gpuVerts.size());
			_wireVB->Resource->Unmap(0, nullptr);
		}
	}

	_wireIB = Renderer::CreateIndexBuffer(_wireIndexCount);
	if (_wireIB && _wireIB->Resource) {
		void* data = nullptr;
		if (SUCCEEDED(_wireIB->Resource->Map(0, nullptr, &data))) {
			memcpy(data, gpuIndices.data(), sizeof(unsigned int) * gpuIndices.size());
			_wireIB->Resource->Unmap(0, nullptr);
		}
	}
}

void ColliderMesh::SyncTransform() {
	if (!_owner) return;

	// ModelRenderer のモデルが変化していたら自動再構築
	ModelRenderer* mr = _owner->GetComponent<ModelRenderer>();
	const MODEL* currentModel = mr ? mr->GetModel() : nullptr;
	if (_localVertices.empty() || currentModel != _cachedModel) {
		RebuildMesh();
	}
	if (_wireIndexCount == 0 && !_uniqueEdges.empty() && Renderer::GetDeviceDX12()) {
		BuildWireframeBuffers();
	}

	KTVECTOR3 pos = _owner->GetWorldPosition() + _center;
	KTVECTOR3 scale = _owner->GetWorldScale();
	KTVECTOR3 right = _owner->GetRight();
	KTVECTOR3 up = _owner->GetUp();
	KTVECTOR3 forward = _owner->GetForward();

	auto transformPoint = [&](const KTVECTOR3& lp) -> KTVECTOR3 {
		return pos
			+ right * (lp.x * scale.x)
			+ up * (lp.y * scale.y)
			+ forward * (lp.z * scale.z);
	};

	float invScaleX = (fabsf(scale.x) > 1e-6f) ? (1.0f / scale.x) : 1.0f;
	float invScaleY = (fabsf(scale.y) > 1e-6f) ? (1.0f / scale.y) : 1.0f;
	float invScaleZ = (fabsf(scale.z) > 1e-6f) ? (1.0f / scale.z) : 1.0f;

	auto transformNormal = [&](const KTVECTOR3& ln) -> KTVECTOR3 {
		return (right * (ln.x * invScaleX)
			+ up * (ln.y * invScaleY)
			+ forward * (ln.z * invScaleZ)).Normalize();
	};

	auto transformEdgeDir = [&](const KTVECTOR3& ld) -> KTVECTOR3 {
		return (right * (ld.x * scale.x)
			+ up * (ld.y * scale.y)
			+ forward * (ld.z * scale.z)).Normalize();
	};

	_worldCenter = transformPoint(_localCenter);

	_worldVertices.resize(_localVertices.size());
	KTVECTOR3 wMin(FLT_MAX, FLT_MAX, FLT_MAX);
	KTVECTOR3 wMax(-FLT_MAX, -FLT_MAX, -FLT_MAX);

	for (size_t i = 0; i < _localVertices.size(); ++i) {
		KTVECTOR3 wp = transformPoint(_localVertices[i]);
		_worldVertices[i] = wp;
		if (wp.x < wMin.x) wMin.x = wp.x;
		if (wp.y < wMin.y) wMin.y = wp.y;
		if (wp.z < wMin.z) wMin.z = wp.z;
		if (wp.x > wMax.x) wMax.x = wp.x;
		if (wp.y > wMax.y) wMax.y = wp.y;
		if (wp.z > wMax.z) wMax.z = wp.z;
	}

	_aabb.min = wMin;
	_aabb.max = wMax;

	_worldFaceNormals.resize(_uniqueFaceNormalsLocal.size());
	for (size_t i = 0; i < _uniqueFaceNormalsLocal.size(); ++i) {
		_worldFaceNormals[i] = transformNormal(_uniqueFaceNormalsLocal[i]);
	}

	_worldEdgeDirs.resize(_uniqueEdgeDirsLocal.size());
	for (size_t i = 0; i < _uniqueEdgeDirsLocal.size(); ++i) {
		_worldEdgeDirs[i] = transformEdgeDir(_uniqueEdgeDirsLocal[i]);
	}

	if ((_oldScale - scale).MagnitudeSqr() > 1e-6f) {
		_hasChangedScale = true;
	} else {
		_hasChangedScale = false;
	}
	_oldScale = scale;
}

bool ColliderMesh::CheckVSSphere(const ColliderSphere* other, CollisionManifold& outCollisionManifold) const {
	if (!this->_aabb.CheckOverlap(other->_aabb)) return false;
	if (_worldVertices.empty() || _faces.empty()) return false;

	KTVECTOR3 sphereCenter = other->GetOwner()->GetWorldPosition();
	float radius = other->_radius;

	RigidBody* rbSphere = other->GetOwner() ? other->GetOwner()->GetComponent<RigidBody>() : nullptr;
	RigidBody* rbMesh = _owner ? _owner->GetComponent<RigidBody>() : nullptr;
	KTVECTOR3 vSphere = (rbSphere && !rbSphere->IsSleeping() && !rbSphere->_isKinematic) ? rbSphere->_velocity : KTVECTOR3(0.0f, 0.0f, 0.0f);
	KTVECTOR3 vMesh = (rbMesh && !rbMesh->IsSleeping() && !rbMesh->_isKinematic) ? rbMesh->_velocity : KTVECTOR3(0.0f, 0.0f, 0.0f);
	KTVECTOR3 relVel = vSphere - vMesh;
	KTVECTOR3 prevSphereCenter = sphereCenter - relVel * (1.0f / 60.0f);

	float minDistSq = FLT_MAX;
	KTVECTOR3 closestPt = _worldCenter;
	float maxSignedPlaneDist = -FLT_MAX;
	float bestApproachScore = -FLT_MAX;
	KTVECTOR3 leastPenetratingNormal(0.0f, 1.0f, 0.0f);

	for (const auto& f : _faces) {
		const KTVECTOR3& a = _worldVertices[f.v0];
		const KTVECTOR3& b = _worldVertices[f.v1];
		const KTVECTOR3& c = _worldVertices[f.v2];

		KTVECTOR3 fn = Cross(b - a, c - a);
		if (Dot(fn, a - _worldCenter) < 0.0f) fn = -fn;
		fn = fn.Normalize();

		float signedDist = Dot(fn, sphereCenter - a);
		if (signedDist > maxSignedPlaneDist) {
			maxSignedPlaneDist = signedDist;
		}
		// 高速侵入時に裏側の面へ抜けるのを防ぐため、移動前の球中心での平面距離も加味して脱出面を選択
		float prevSignedDist = Dot(fn, prevSphereCenter - a);
		float score = (relVel.MagnitudeSqr() > 0.25f && Dot(fn, relVel) < 0.0f)
			? (std::max)(signedDist, prevSignedDist)
			: signedDist;
		if (score > bestApproachScore) {
			bestApproachScore = score;
			leastPenetratingNormal = fn;
		}

		KTVECTOR3 pt = ClosestPointOnTriangle(sphereCenter, a, b, c);
		float dSq = (sphereCenter - pt).MagnitudeSqr();
		if (dSq < minDistSq) {
			minDistSq = dSq;
			closestPt = pt;
		}
	}

	bool insideHull = (maxSignedPlaneDist <= 0.0f);
	if (!insideHull && minDistSq > radius * radius) {
		return false;
	}

	outCollisionManifold.hasCollision = true;
	outCollisionManifold.a = const_cast<ColliderSphere*>(other);
	outCollisionManifold.b = const_cast<ColliderMesh*>(this);

	if (insideHull) {
		float planeDist = -FLT_MAX;
		for (const auto& v : _worldVertices) {
			float p = Dot(leastPenetratingNormal, v);
			if (p > planeDist) planeDist = p;
		}
		float sphereProj = Dot(leastPenetratingNormal, sphereCenter);
		float pen = (std::max)(0.0f, (planeDist - sphereProj) + radius);
		outCollisionManifold.normal = leastPenetratingNormal;
		outCollisionManifold.penetrationDepth = pen;
		ContactPoint cp;
		cp.position = sphereCenter + leastPenetratingNormal * (planeDist - sphereProj);
		cp.penetration = pen;
		outCollisionManifold.contacts.push_back(cp);
	} else {
		float dist = sqrtf(minDistSq);
		KTVECTOR3 diff = sphereCenter - closestPt;
		outCollisionManifold.normal = (dist > 1e-5f) ? (diff / dist) : leastPenetratingNormal;
		outCollisionManifold.penetrationDepth = radius - dist;
		ContactPoint cp;
		cp.position = closestPt;
		cp.penetration = outCollisionManifold.penetrationDepth;
		outCollisionManifold.contacts.push_back(cp);
	}

	return true;
}

bool ColliderMesh::CheckVSCapsule(const ColliderCapsule* other, CollisionManifold& outCollisionManifold) const {
	if (!this->_aabb.CheckOverlap(other->_aabb)) return false;
	if (_worldVertices.empty() || _faces.empty()) return false;

	float radius = other->GetRadius();
	float cylH = (std::max)(0.0f, other->GetHeight() - 2.0f * radius);
	KTVECTOR3 up = other->GetOwner()->GetUp();
	KTVECTOR3 segA = other->GetOwner()->GetWorldPosition() - up * (cylH * 0.5f);
	KTVECTOR3 segB = other->GetOwner()->GetWorldPosition() + up * (cylH * 0.5f);

	RigidBody* rbCap = other->GetOwner() ? other->GetOwner()->GetComponent<RigidBody>() : nullptr;
	RigidBody* rbMesh = _owner ? _owner->GetComponent<RigidBody>() : nullptr;
	KTVECTOR3 vCap = (rbCap && !rbCap->IsSleeping() && !rbCap->_isKinematic) ? rbCap->_velocity : KTVECTOR3(0.0f, 0.0f, 0.0f);
	KTVECTOR3 vMesh = (rbMesh && !rbMesh->IsSleeping() && !rbMesh->_isKinematic) ? rbMesh->_velocity : KTVECTOR3(0.0f, 0.0f, 0.0f);
	KTVECTOR3 relVel = vCap - vMesh;

	// カプセル中心線分と各三角形・各エッジの最短距離を評価
	float bestDistSq = FLT_MAX;
	KTVECTOR3 bestSegPt = segA;
	KTVECTOR3 bestMeshPt = _worldCenter;

	const int SAMPLES = 6;
	for (int s = 0; s <= SAMPLES; ++s) {
		float t = (float)s / (float)SAMPLES;
		KTVECTOR3 p = segA + (segB - segA) * t;
		for (const auto& f : _faces) {
			KTVECTOR3 q = ClosestPointOnTriangle(p, _worldVertices[f.v0], _worldVertices[f.v1], _worldVertices[f.v2]);
			KTVECTOR3 ab = segB - segA;
			float abLenSq = ab.MagnitudeSqr();
			float u = (abLenSq > 1e-6f) ? Clamp(Dot(q - segA, ab) / abLenSq, 0.0f, 1.0f) : 0.0f;
			KTVECTOR3 pRefined = segA + ab * u;
			KTVECTOR3 qRefined = ClosestPointOnTriangle(pRefined, _worldVertices[f.v0], _worldVertices[f.v1], _worldVertices[f.v2]);

			float dSq = (pRefined - qRefined).MagnitudeSqr();
			if (dSq < bestDistSq) {
				bestDistSq = dSq;
				bestSegPt = pRefined;
				bestMeshPt = qRefined;
			}
		}
	}

	// メッシュのユニークエッジとカプセル軸線分の最近接点も評価（エッジ同士の衝突漏れ防止）
	for (const auto& edge : _uniqueEdges) {
		KTVECTOR3 cSeg, cEdge;
		ClosestPointSegSeg(segA, segB, _worldVertices[edge.first], _worldVertices[edge.second], cSeg, cEdge);
		float dSq = (cSeg - cEdge).MagnitudeSqr();
		if (dSq < bestDistSq) {
			bestDistSq = dSq;
			bestSegPt = cSeg;
			bestMeshPt = cEdge;
		}
	}

	// bestSegPt が凸包内部に入っているかもチェック（高速移動時の接近方向を考慮）
	KTVECTOR3 prevSegPt = bestSegPt - relVel * (1.0f / 60.0f);
	float maxSignedPlaneDist = -FLT_MAX;
	float bestApproachScore = -FLT_MAX;
	KTVECTOR3 leastPenetratingNormal(0.0f, 1.0f, 0.0f);
	for (const auto& f : _faces) {
		const KTVECTOR3& a = _worldVertices[f.v0];
		const KTVECTOR3& b = _worldVertices[f.v1];
		const KTVECTOR3& c = _worldVertices[f.v2];
		KTVECTOR3 fn = Cross(b - a, c - a);
		if (Dot(fn, a - _worldCenter) < 0.0f) fn = -fn;
		fn = fn.Normalize();
		float sd = Dot(fn, bestSegPt - a);
		if (sd > maxSignedPlaneDist) {
			maxSignedPlaneDist = sd;
		}
		float prevSd = Dot(fn, prevSegPt - a);
		float score = (relVel.MagnitudeSqr() > 0.25f && Dot(fn, relVel) < 0.0f)
			? (std::max)(sd, prevSd)
			: sd;
		if (score > bestApproachScore) {
			bestApproachScore = score;
			leastPenetratingNormal = fn;
		}
	}

	bool insideHull = (maxSignedPlaneDist <= 0.0f);
	if (!insideHull && bestDistSq > radius * radius) {
		return false;
	}

	outCollisionManifold.hasCollision = true;
	outCollisionManifold.a = const_cast<ColliderCapsule*>(other);
	outCollisionManifold.b = const_cast<ColliderMesh*>(this);

	if (insideHull) {
		float planeDist = -FLT_MAX;
		for (const auto& v : _worldVertices) {
			float p = Dot(leastPenetratingNormal, v);
			if (p > planeDist) planeDist = p;
		}
		float segProj = Dot(leastPenetratingNormal, bestSegPt);
		float pen = (std::max)(0.0f, (planeDist - segProj) + radius);
		outCollisionManifold.normal = leastPenetratingNormal;
		outCollisionManifold.penetrationDepth = pen;
		ContactPoint cp;
		cp.position = bestSegPt + leastPenetratingNormal * (planeDist - segProj);
		cp.penetration = pen;
		outCollisionManifold.contacts.push_back(cp);
	} else {
		float dist = sqrtf(bestDistSq);
		KTVECTOR3 diff = bestSegPt - bestMeshPt;
		outCollisionManifold.normal = (dist > 1e-5f) ? (diff / dist) : leastPenetratingNormal;
		outCollisionManifold.penetrationDepth = radius - dist;
		ContactPoint cp;
		cp.position = bestMeshPt;
		cp.penetration = outCollisionManifold.penetrationDepth;
		outCollisionManifold.contacts.push_back(cp);

		// カプセルが横向きに接地している場合、もう一方の端点も接触していれば2点目の接触点を追加して傾きを防ぐ
		KTVECTOR3 endPts[2] = { segA, segB };
		for (int i = 0; i < 2; ++i) {
			if ((endPts[i] - bestSegPt).MagnitudeSqr() < 0.01f) continue;
			float endMinSq = FLT_MAX;
			KTVECTOR3 endMeshPt = bestMeshPt;
			for (const auto& f : _faces) {
				KTVECTOR3 q = ClosestPointOnTriangle(endPts[i], _worldVertices[f.v0], _worldVertices[f.v1], _worldVertices[f.v2]);
				float dSq = (endPts[i] - q).MagnitudeSqr();
				if (dSq < endMinSq) {
					endMinSq = dSq;
					endMeshPt = q;
				}
			}
			if (endMinSq <= radius * radius) {
				ContactPoint cp2;
				cp2.position = endMeshPt;
				cp2.penetration = (std::max)(0.0f, radius - sqrtf(endMinSq));
				outCollisionManifold.contacts.push_back(cp2);
			}
		}
	}

	return true;
}

namespace {
	// 2つの凸多面体頂点群のSAT（分離軸定理）共通ソルバー
	bool SolveConvexPolyhedraSAT(
		Collider* colA_other,
		const std::vector<KTVECTOR3>& vertsA,
		const KTVECTOR3& centerA,
		const std::vector<KTVECTOR3>& normalsA,
		const std::vector<KTVECTOR3>& edgesA,
		Collider* colB_self,
		const std::vector<KTVECTOR3>& vertsB,
		const KTVECTOR3& centerB,
		const std::vector<KTVECTOR3>& normalsB,
		const std::vector<KTVECTOR3>& edgesB,
		CollisionManifold& outManifold)
	{
		// 相対速度と1ステップ前の予測中心位置（高速衝突で中心を追い越した際の法線逆転を防止）
		RigidBody* rbA = (colA_other && colA_other->GetOwner()) ? colA_other->GetOwner()->GetComponent<RigidBody>() : nullptr;
		RigidBody* rbB = (colB_self && colB_self->GetOwner()) ? colB_self->GetOwner()->GetComponent<RigidBody>() : nullptr;
		KTVECTOR3 vA = (rbA && !rbA->IsSleeping() && !rbA->_isKinematic) ? rbA->_velocity : KTVECTOR3(0.0f, 0.0f, 0.0f);
		KTVECTOR3 vB = (rbB && !rbB->IsSleeping() && !rbB->_isKinematic) ? rbB->_velocity : KTVECTOR3(0.0f, 0.0f, 0.0f);
		KTVECTOR3 relVel = vA - vB;
		constexpr float LOOKBACK_DT = 1.0f / 60.0f;
		KTVECTOR3 currCenterDelta = centerA - centerB;
		KTVECTOR3 prevCenterDelta = (centerA - vA * LOOKBACK_DT) - (centerB - vB * LOOKBACK_DT);

		float minOverlap = FLT_MAX;
		KTVECTOR3 bestAxis(0.0f, 1.0f, 0.0f);

		auto testAxis = [&](const KTVECTOR3& rawAxis, bool isEdgeCrossAxis) -> bool {
			float lenSq = rawAxis.MagnitudeSqr();
			if (lenSq < 1e-6f) return true;
			KTVECTOR3 axis = rawAxis / sqrtf(lenSq);

			float minA = FLT_MAX, maxA = -FLT_MAX;
			for (const auto& v : vertsA) {
				float p = Dot(v, axis);
				if (p < minA) minA = p;
				if (p > maxA) maxA = p;
			}

			float minB = FLT_MAX, maxB = -FLT_MAX;
			for (const auto& v : vertsB) {
				float p = Dot(v, axis);
				if (p < minB) minB = p;
				if (p > maxB) maxB = p;
			}

			if (maxA < minB || maxB < minA) {
				return false; // 分離軸発見
			}

			// 真の最小分離距離 (Minimum Translation Distance):
			// dPos: Aを +axis 方向へ押し出す距離 (maxB - minA)
			// dNeg: Aを -axis 方向へ押し出す距離 (maxA - minB)
			float dPos = maxB - minA;
			float dNeg = maxA - minB;

			float prevProj = Dot(prevCenterDelta, axis);
			float currProj = Dot(currCenterDelta, axis);
			float relVelProj = Dot(relVel, axis);

			float overlap = 0.0f;
			KTVECTOR3 orientedAxis = axis;

			// 高速衝突によって1フレーム内で中心座標が交差した場合、移動前の進行方向側の表面へ押し戻す
			if (prevProj * currProj < 0.0f && fabsf(relVelProj) > 0.5f) {
				if (prevProj >= 0.0f) {
					overlap = dPos;
					orientedAxis = axis;
				} else {
					overlap = dNeg;
					orientedAxis = -axis;
				}
			} else if (dPos <= dNeg) {
				overlap = dPos;
				orientedAxis = axis;
			} else {
				overlap = dNeg;
				orientedAxis = -axis;
			}

			// エッジ外積軸は数値誤差で面法線とほぼ同値になることがあるため、面法線より明確に浅い場合のみ採用
			float effectiveOverlap = isEdgeCrossAxis ? (overlap * 1.04f + 0.002f) : overlap;
			if (effectiveOverlap < minOverlap) {
				minOverlap = overlap;
				bestAxis = orientedAxis;
			}
			return true;
		};

		for (const auto& n : normalsB) {
			if (!testAxis(n, false)) return false;
		}
		for (const auto& n : normalsA) {
			if (!testAxis(n, false)) return false;
		}
		for (const auto& eB : edgesB) {
			for (const auto& eA : edgesA) {
				KTVECTOR3 crossAxis = Cross(eB, eA);
				if (crossAxis.MagnitudeSqr() < 1e-5f) continue;
				if (!testAxis(crossAxis, true)) return false;
			}
		}

		// 法線向きを B(self) -> A(other) に最終確認（移動前の相対中心を優先）
		KTVECTOR3 refDelta = (prevCenterDelta.MagnitudeSqr() > 1e-6f) ? prevCenterDelta : currCenterDelta;
		if (Dot(refDelta, bestAxis) < 0.0f) {
			bestAxis = -bestAxis;
		}
		if (fabsf(bestAxis.x) < 1e-5f) bestAxis.x = 0.0f;
		if (fabsf(bestAxis.y) < 1e-5f) bestAxis.y = 0.0f;
		if (fabsf(bestAxis.z) < 1e-5f) bestAxis.z = 0.0f;
		bestAxis = bestAxis.Normalize();

		// 正しい向きに揃えた bestAxis 上での A の最小投影値と B の最大投影値を再計算
		float minProjA = FLT_MAX;
		for (const auto& v : vertsA) {
			float p = Dot(v, bestAxis);
			if (p < minProjA) minProjA = p;
		}
		float maxProjB = -FLT_MAX;
		for (const auto& v : vertsB) {
			float p = Dot(v, bestAxis);
			if (p > maxProjB) maxProjB = p;
		}
		minOverlap = (std::max)(0.0f, maxProjB - minProjA);

		outManifold.hasCollision = true;
		outManifold.a = colA_other;
		outManifold.b = colB_self;
		outManifold.normal = bestAxis;
		outManifold.penetrationDepth = minOverlap;

		// 法線に直交する接平面基底 (tangentU, tangentV) を構築
		KTVECTOR3 tangentU = (fabsf(bestAxis.y) < 0.9f)
			? Cross(bestAxis, KTVECTOR3(0.0f, 1.0f, 0.0f)).Normalize()
			: Cross(bestAxis, KTVECTOR3(1.0f, 0.0f, 0.0f)).Normalize();
		KTVECTOR3 tangentV = Cross(bestAxis, tangentU).Normalize();

		const float contactEps = (std::max)(0.04f, minOverlap + 0.02f);

		// A と B の接触面付近の頂点群から、接平面 (u, v) 上での各バウンディング領域を算出
		float minUA = FLT_MAX, maxUA = -FLT_MAX, minVA = FLT_MAX, maxVA = -FLT_MAX;
		std::vector<KTVECTOR3> supportA;
		for (const auto& v : vertsA) {
			if (Dot(v, bestAxis) <= minProjA + contactEps) {
				supportA.push_back(v);
				float u = Dot(v, tangentU);
				float w = Dot(v, tangentV);
				if (u < minUA) minUA = u;
				if (u > maxUA) maxUA = u;
				if (w < minVA) minVA = w;
				if (w > maxVA) maxVA = w;
			}
		}

		float minUB = FLT_MAX, maxUB = -FLT_MAX, minVB = FLT_MAX, maxVB = -FLT_MAX;
		std::vector<KTVECTOR3> supportB;
		for (const auto& v : vertsB) {
			if (Dot(v, bestAxis) >= maxProjB - contactEps) {
				supportB.push_back(v);
				float u = Dot(v, tangentU);
				float w = Dot(v, tangentV);
				if (u < minUB) minUB = u;
				if (u > maxUB) maxUB = u;
				if (w < minVB) minVB = w;
				if (w > maxVB) maxVB = w;
			}
		}

		// 接平面上での A と B の共通接触領域 (クリッピング領域)
		float clipMinU = (std::max)(minUA, minUB);
		float clipMaxU = (std::min)(maxUA, maxUB);
		float clipMinV = (std::max)(minVA, minVB);
		float clipMaxV = (std::min)(maxVA, maxVB);
		if (clipMinU > clipMaxU) {
			float midU = (clipMinU + clipMaxU) * 0.5f;
			clipMinU = clipMaxU = midU;
		}
		if (clipMinV > clipMaxV) {
			float midV = (clipMinV + clipMaxV) * 0.5f;
			clipMinV = clipMaxV = midV;
		}

		struct CandidateContact {
			KTVECTOR3 pos;
			float u;
			float v;
			float penetration;
		};
		std::vector<CandidateContact> candidates;
		float contactPlaneProj = (minProjA + maxProjB) * 0.5f;

		auto addCandidate = [&](float u, float v, float pen) {
			// 共通接触領域内にクランプ（巨大な床の四隅がそのまま接触点になるのを防ぐ）
			float cu = Clamp(u, clipMinU, clipMaxU);
			float cv = Clamp(v, clipMinV, clipMaxV);
			float cpen = Clamp(pen, 0.0f, minOverlap);
			if (cpen <= 0.0f) cpen = minOverlap;

			// 接平面上でほぼ同位置の重複候補を統合（より深い方を残す）
			for (auto& existing : candidates) {
				float du = existing.u - cu;
				float dv = existing.v - cv;
				if (du * du + dv * dv < 1e-4f) {
					if (cpen > existing.penetration) {
						existing.penetration = cpen;
					}
					return;
				}
			}

			KTVECTOR3 worldPt = bestAxis * contactPlaneProj + tangentU * cu + tangentV * cv;
			candidates.push_back({ worldPt, cu, cv, cpen });
		};

		for (const auto& v : supportA) {
			float p = Dot(v, bestAxis);
			float pen = (std::max)(0.0f, maxProjB - p);
			addCandidate(Dot(v, tangentU), Dot(v, tangentV), pen);
		}
		for (const auto& v : supportB) {
			float p = Dot(v, bestAxis);
			float pen = (std::max)(0.0f, p - minProjA);
			addCandidate(Dot(v, tangentU), Dot(v, tangentV), pen);
		}

		// 候補点が4点を超える場合は、最大広がり（Support Polygon面積最大化）を持つ4点を選出
		if (candidates.size() <= 4) {
			for (const auto& c : candidates) {
				ContactPoint cp;
				cp.position = c.pos;
				cp.penetration = c.penetration;
				outManifold.contacts.push_back(cp);
			}
		} else {
			int idx[4] = { 0, -1, -1, -1 };
			// 1点目: 最も貫通が深い（または端にある）点
			float bestScore0 = -FLT_MAX;
			float centerU = (clipMinU + clipMaxU) * 0.5f;
			float centerV = (clipMinV + clipMaxV) * 0.5f;
			for (int i = 0; i < (int)candidates.size(); ++i) {
				float distSq = (candidates[i].u - centerU) * (candidates[i].u - centerU) +
					(candidates[i].v - centerV) * (candidates[i].v - centerV);
				float score = candidates[i].penetration * 10.0f + distSq;
				if (score > bestScore0) {
					bestScore0 = score;
					idx[0] = i;
				}
			}

			// 2点目: 1点目から接平面上で最も遠い点
			float maxDistSq1 = -1.0f;
			for (int i = 0; i < (int)candidates.size(); ++i) {
				if (i == idx[0]) continue;
				float du = candidates[i].u - candidates[idx[0]].u;
				float dv = candidates[i].v - candidates[idx[0]].v;
				float dSq = du * du + dv * dv;
				if (dSq > maxDistSq1) {
					maxDistSq1 = dSq;
					idx[1] = i;
				}
			}

			// 3点目: 線分(0-1)との2D三角形面積が最大になる点
			float maxArea2 = -1.0f;
			if (idx[1] >= 0) {
				float u01 = candidates[idx[1]].u - candidates[idx[0]].u;
				float v01 = candidates[idx[1]].v - candidates[idx[0]].v;
				for (int i = 0; i < (int)candidates.size(); ++i) {
					if (i == idx[0] || i == idx[1]) continue;
					float u0i = candidates[i].u - candidates[idx[0]].u;
					float v0i = candidates[i].v - candidates[idx[0]].v;
					float area = fabsf(u01 * v0i - v01 * u0i);
					if (area > maxArea2) {
						maxArea2 = area;
						idx[2] = i;
					}
				}
			}

			// 4点目: 既存の3辺から最も離れた外側の点
			float maxArea3 = -1.0f;
			if (idx[2] >= 0) {
				int tri[3] = { idx[0], idx[1], idx[2] };
				for (int i = 0; i < (int)candidates.size(); ++i) {
					if (i == idx[0] || i == idx[1] || i == idx[2]) continue;
					float minEdgeDist = FLT_MAX;
					for (int e = 0; e < 3; ++e) {
						int a = tri[e];
						int b = tri[(e + 1) % 3];
						float eu = candidates[b].u - candidates[a].u;
						float ev = candidates[b].v - candidates[a].v;
						float pu = candidates[i].u - candidates[a].u;
						float pv = candidates[i].v - candidates[a].v;
						float area = fabsf(eu * pv - ev * pu);
						if (area < minEdgeDist) minEdgeDist = area;
					}
					if (minEdgeDist > maxArea3) {
						maxArea3 = minEdgeDist;
						idx[3] = i;
					}
				}
			}

			for (int k = 0; k < 4; ++k) {
				if (idx[k] >= 0) {
					ContactPoint cp;
					cp.position = candidates[idx[k]].pos;
					cp.penetration = candidates[idx[k]].penetration;
					outManifold.contacts.push_back(cp);
				}
			}
		}

		if (outManifold.contacts.empty()) {
			ContactPoint cp;
			cp.position = bestAxis * contactPlaneProj
				+ tangentU * ((clipMinU + clipMaxU) * 0.5f)
				+ tangentV * ((clipMinV + clipMaxV) * 0.5f);
			cp.penetration = minOverlap;
			outManifold.contacts.push_back(cp);
		}

		return true;
	}
}

bool ColliderMesh::CheckVSOBB(const ColliderBox* other, CollisionManifold& outCollisionManifold) const {
	if (!this->_aabb.CheckOverlap(other->_aabb)) return false;
	if (_worldVertices.empty()) return false;

	KTVECTOR3 bCenter = other->GetOwner()->GetWorldPosition();
	KTVECTOR3 ax0 = other->_axis[0] * other->_extents.x;
	KTVECTOR3 ax1 = other->_axis[1] * other->_extents.y;
	KTVECTOR3 ax2 = other->_axis[2] * other->_extents.z;

	std::vector<KTVECTOR3> boxVerts = {
		bCenter - ax0 - ax1 - ax2,
		bCenter + ax0 - ax1 - ax2,
		bCenter + ax0 + ax1 - ax2,
		bCenter - ax0 + ax1 - ax2,
		bCenter - ax0 - ax1 + ax2,
		bCenter + ax0 - ax1 + ax2,
		bCenter + ax0 + ax1 + ax2,
		bCenter - ax0 + ax1 + ax2
	};
	std::vector<KTVECTOR3> boxAxes = {
		other->_axis[0],
		other->_axis[1],
		other->_axis[2]
	};

	return SolveConvexPolyhedraSAT(
		const_cast<ColliderBox*>(other),
		boxVerts,
		bCenter,
		boxAxes,
		boxAxes,
		const_cast<ColliderMesh*>(this),
		_worldVertices,
		_worldCenter,
		_worldFaceNormals,
		_worldEdgeDirs,
		outCollisionManifold
	);
}

bool ColliderMesh::CheckVSMesh(const ColliderMesh* other, CollisionManifold& outCollisionManifold) const {
	if (!this->_aabb.CheckOverlap(other->_aabb)) return false;
	if (_worldVertices.empty() || other->_worldVertices.empty()) return false;

	return SolveConvexPolyhedraSAT(
		const_cast<ColliderMesh*>(other),
		other->_worldVertices,
		other->_worldCenter,
		other->_worldFaceNormals,
		other->_worldEdgeDirs,
		const_cast<ColliderMesh*>(this),
		_worldVertices,
		_worldCenter,
		_worldFaceNormals,
		_worldEdgeDirs,
		outCollisionManifold
	);
}

KTMATRIX3 ColliderMesh::ComputeLocalInertiaTensor(float mass) {
	if (mass <= 0.0f || !_owner) return KTMATRIX3::Zero();
	KTVECTOR3 s = _owner->GetWorldScale();
	float ex = (std::max)(0.05f, _localHalfExtents.x * s.x);
	float ey = (std::max)(0.05f, _localHalfExtents.y * s.y);
	float ez = (std::max)(0.05f, _localHalfExtents.z * s.z);

	float ix = (1.0f / 12.0f) * mass * (ey * ey + ez * ez) * 4.0f;
	float iy = (1.0f / 12.0f) * mass * (ex * ex + ez * ez) * 4.0f;
	float iz = (1.0f / 12.0f) * mass * (ex * ex + ey * ey) * 4.0f;
	return KTMATRIX3(
		ix, 0.0f, 0.0f,
		0.0f, iy, 0.0f,
		0.0f, 0.0f, iz
	);
}

void ColliderMesh::ShowUI() {
	ImGui::Checkbox("_wasOverlap", &_wasOverlap);
	ImGui::DragFloat3("Center Offset", &_center.x, 0.05f);
	ImGui::Text("Convex Hull Vertices: %d", (int)_localVertices.size());
	ImGui::Text("Convex Hull Faces:    %d", (int)_faces.size());
	ImGui::Text("Wireframe Edges:      %d", (int)_uniqueEdges.size());
	if (ImGui::Button("Rebuild Mesh Collider")) {
		RebuildMesh();
		SyncTransform();
	}
}
