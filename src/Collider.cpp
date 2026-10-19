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

void ClosestPointSegSeg(const KTVECTOR3& s1, const KTVECTOR3& e1, const KTVECTOR3& s2, const KTVECTOR3& e2, KTVECTOR3& c1, KTVECTOR3& c2){
	float s, t;//}ﾏ・

	KTVECTOR3 d1 = e1 - s1;	//ﾌ富Ng
	KTVECTOR3 d2 = e2 - s2;
	KTVECTOR3 r = s1 - s2;	//n_mﾌベNgi21j

	float lenSqD1 = d1.MagnitudeSqr();//ﾌ・
	float lenSqD2 = d2.MagnitudeSqr();
	float f = Dot(d2, r);

	const float EPSILON = 1e-6f;

	if (lenSqD1 <= EPSILON && lenSqD2 <= EPSILON) {//_ﾉ退夏れ合
		s = t = 0.0f;
		c1 = s1;
		c2 = s2;
		return;
	}
	if (lenSqD1 <= EPSILON) {	//1_ﾉ退夏れ合
		s = 0.0f;
		t = Clamp(f / lenSqD2, 0.0f, 1.0f);
	}
	else {	//1ﾍ点ﾈ・
		float c = Dot(d1, r);
		if (lenSqD2 <= EPSILON) {	//2_ﾉ退・
			t = 0.0f;
			s = Clamp(-c / lenSqD1, 0.0f, 1.0f);
		}
		else {	//ﾇのま・
			float b = Dot(d1, d2);	//ﾌ富Ngmﾌ・
			float denom = lenSqD1 * lenSqD2 - b * b;	//Nﾌ故・

			if (denom != 0.0f) {//sﾅなれ合
				s = Clamp((b * f - c * lenSqD2) / denom, 0.0f, 1.0f);
			}
			else {//ﾍ不
				s = 0.0f;
			}
			t = (b * s + f) / lenSqD2;

			//t0~1o鼾・再計Z
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
	//_owner->_transform._scaleﾌ慰大きlｽ映
	float tempScale = _owner->_transform._scale.x;
	if (tempScale < _owner->_transform._scale.y) {
		tempScale = _owner->_transform._scale.y;
	}
	if (tempScale < _owner->_transform._scale.z) {
		tempScale = _owner->_transform._scale.z;
	}
	//ﾚ（SphereMesh逕ｼaｽ映j
	Sphere* sp;
	if (sp = _owner->GetComponent<Sphere>()) {
		_radius = sp->GetRadius() * tempScale;
	}
	else
		_radius = tempScale * 0.5f;

	//AABBXV
	_aabb.min = _owner->_transform._position - KTVECTOR3(_radius, _radius, _radius);
	_aabb.max = _owner->_transform._position + KTVECTOR3(_radius, _radius, _radius);

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

	//Sﾔの・
	float distanceSqr = (this->_owner->_transform._position -
		other->_owner->_transform._position).MagnitudeSqr();

	//aﾌ和ﾌ・
	float radiusSumSqr = (this->_radius + other->_radius) * (this->_radius + other->_radius);

	if (distanceSqr <= radiusSumSqr) {//ﾄ・
		outCollisionManifold.penetrationDepth = sqrtf(radiusSumSqr) - sqrtf(distanceSqr);
		outCollisionManifold.normal = (other->_owner->_transform._position -
			this->_owner->_transform._position).Normalize();
		KTVECTOR3 cpa;//Aﾌ接触_
		cpa = other->_owner->_transform._position -
			_radius * outCollisionManifold.normal;

		KTVECTOR3 cpb;//Bﾌ接触_
		cpb = this->_owner->_transform._position +
			_radius * outCollisionManifold.normal;

		ContactPoint cp;//ﾚ触_mﾌ胆ﾕ突暇用
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

	KTVECTOR3 bPos = other->GetOwner()->_transform._position;	//OBBﾌ晃
	KTVECTOR3 cPos = this->_owner->_transform._position;		//Sphereﾌ晃

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

	_center = _owner->_transform._position;
	_axis[0] = _owner->GetRight();
	_axis[1] = _owner->GetUp();
	_axis[2] = _owner->GetForward();

	_extents = _owner->_transform._scale * 0.5f;

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
	//[JXV
	_center = _owner->_transform._position;
	_axis[0] = _owner->GetRight();
	_axis[1] = _owner->GetUp();
	_axis[2] = _owner->GetForward();

	_extents = _owner->_transform._scale * 0.5f;

	//AABBXV
	KTVECTOR3 r;
	r.x = fabsf(_axis[0].x) * _extents.x + fabsf(_axis[1].x) * _extents.y + fabsf(_axis[2].x) * _extents.z;
	r.y = fabsf(_axis[0].y) * _extents.x + fabsf(_axis[1].y) * _extents.y + fabsf(_axis[2].y) * _extents.z;
	r.z = fabsf(_axis[0].z) * _extents.x + fabsf(_axis[1].z) * _extents.y + fabsf(_axis[2].z) * _extents.z;

	_aabb.min = _owner->_transform._position - r;
	_aabb.max = _owner->_transform._position + r;

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

	float minOverlap = FLT_MAX;	//ﾅ称重ﾈ・
	KTVECTOR3 bestAxis;			//ﾅ称重ﾈり軸

	//eﾉつて兎ﾄチFbN
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

	//Oﾏでで・mF
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

	//ﾕ突醜V

	KTVECTOR3 centerDelta = other->_center - _center;
	if (Dot(centerDelta, bestAxis) < 0.0f)
		bestAxis = -bestAxis;

	manifold.hasCollision = true;
	manifold.normal = bestAxis.Normalize();
	manifold.penetrationDepth = minOverlap;


	//ﾕ突点ﾌ計Z
	FixedList<KTVECTOR3, 16> contactPolygon = ComputeContactPolygon(this, other, manifold.normal);
	if (!contactPolygon.empty()) {
		for (auto& p : contactPolygon) {
			ContactPoint cp;
			cp.position = p;
			cp.penetration = minOverlap;	//ﾆりあlA關ｳmﾉなつ別に算o
			manifold.contacts.push_back(cp);
		}
	}
	else {//Nbvﾅ場合ﾌ淑ﾅ近傍_j
		KTVECTOR3 pointOnA = _center + manifold.normal * (_extents.x + _extents.y + _extents.z);
		KTVECTOR3 pointOnB = other->_center - manifold.normal * (other->_extents.x + other->_extents.y + other->_extents.z);
		ContactPoint cp;
		cp.position = (pointOnA + pointOnB) * 0.5f;
		cp.penetration = minOverlap;
		manifold.contacts.push_back(cp);
	}
	
	//Sﾄの芝重ﾈてので衝突て・
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

	//gﾌ兎
	float centerA = Dot(_center, L);
	float rA = _extents.x * fabs(Dot(_axis[0], L)) +
		_extents.y * fabs(Dot(_axis[1], L)) +
		_extents.z * fabs(Dot(_axis[2], L));

	//ﾌ兎
	float centerB = Dot(other->_center, L);
	float rB = other->_extents.x * fabs(Dot(other->_axis[0], L)) +
		other->_extents.y * fabs(Dot(other->_axis[1], L)) +
		other->_extents.z * fabs(Dot(other->_axis[2], L));

	//S
	float distance = fabs(centerA - centerB);

	//ﾌ有
	return distance <= (rA + rB);

}

bool ColliderBox::OverlapOnAxis(const ColliderBox* other, const KTVECTOR3& axis, float& outOverlap) const
{
	if (axis.Magnitude() < 1e-6f) {
		outOverlap = FLT_MAX;
		return true;
	}

	KTVECTOR3 L = axis.Normalize();

	//gﾌ兎
	float centerA = Dot(_center, L);
	float rA = _extents.x * fabs(Dot(_axis[0], L)) +
		_extents.y * fabs(Dot(_axis[1], L)) +
		_extents.z * fabs(Dot(_axis[2], L));

	//ﾌ兎
	float centerB = Dot(other->_center, L);
	float rB = other->_extents.x * fabs(Dot(other->_axis[0], L)) +
		other->_extents.y * fabs(Dot(other->_axis[1], L)) +
		other->_extents.z * fabs(Dot(other->_axis[2], L));

	//S
	float distance = fabs(centerA - centerB);
	outOverlap = (rA + rB) - distance;

	//ﾌ有
	return distance <= (rA + rB);
}

FixedList<KTVECTOR3, 4> ColliderBox::GetFaceVertices(const ColliderBox* box, int axisIndex, int sign){
	FixedList<KTVECTOR3, 4> verts;

	// extents ﾌ各o
	float ex = box->_extents.x;
	float ey = box->_extents.y;
	float ez = box->_extents.z;

	float extent = (axisIndex == 0) ? ex : (axisIndex == 1 ? ey : ez);

	// face center
	KTVECTOR3 faceCenter = box->_center + box->_axis[axisIndex] * (extent * (float)sign);

	// face ﾌゼJ2
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
			//  -> B ﾇ・
			out.push_back(B);
		}
		else if (inA && !inB) {
			// A inside, B outside -> _ﾇ・
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
			// O -> ﾈ・
		}
	}

	return out;
}


FixedList<KTVECTOR3, 16> ColliderBox::ComputeContactPolygon(const ColliderBox* refBox, const ColliderBox* incBox, const KTVECTOR3& collisionNormal){
	//QﾆボbNXirefBoxjﾑ参ﾆ面の・
	//Qﾆ・bestAxis ﾉ最め・(abs dot ﾅ・ I
	int refAxis = 0;
	float bestDot = fabs(Dot(refBox->_axis[0], collisionNormal));
	for (int i = 1; i < 3; ++i) {
		float d = fabs(Dot(refBox->_axis[i], collisionNormal));
		if (d > bestDot) { bestDot = d; refAxis = i; }
	}

	//@ﾆ拶t
	float s = Dot(collisionNormal, refBox->_axis[refAxis]);
	int refSign = (s >= 0.0f) ? +1 : -1;

	//|S = Qﾆ面・_
	FixedList<KTVECTOR3, 16> poly;
	auto initalVerts =  GetFaceVertices(refBox, refAxis, refSign);

	//^墲ｹ驍ｽﾟにコs[
	for(const auto& v : initalVerts){
		poly.push_back(v);
	}

	//CVfg{bNX(incBox)6ﾊよ得
	FixedList<Plane, 8> incPlanes = GetOBBPlanes(incBox);

	//polyeﾊで硝クbv
	for (const Plane& pl : incPlanes) {
		poly = ClipPolygonAgainstPlane(poly, pl);
		if (poly.empty()) break;
	}

	return poly; // ﾈ・bvﾅ照・
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
	//GameObjectﾌ焦更V
	// _owner->_transform._scale ﾌ慰大きlｽ映icﾝＩﾟの均店P[j
	float tempScaleRad = _owner->_transform._scale.x;
	float tempScaleHeight = _owner->_transform._scale.y;
	if (tempScaleRad < _owner->_transform._scale.z) tempScaleRad = _owner->_transform._scale.z;

	// ﾚ（CapsuleR|[lgjp[^ｽ映
	Capsule* cap = _owner->GetComponent<Capsule>();
	if (cap) {
		_radius = cap->_radius * tempScaleRad;
		_height = cap->_height * tempScaleHeight;
	}
	else {
		_radius = tempScaleRad * 0.5f;
		_height = tempScaleHeight * 2.0f;
	}

	//AABBﾌ更V
	float halfCylHeight = (std::max)(0.0f, _height - 2.0f * _radius) * 0.5f;

	//JvZﾌ互・iUpxNgj謫ｾ
	KTVECTOR3 up = _owner->GetUp();

	//eiX, Y, Zjﾉ対てどれだLﾑてるかvZAaｫ・
	KTVECTOR3 extents;
	extents.x = std::abs(up.x) * halfCylHeight + _radius;
	extents.y = std::abs(up.y) * halfCylHeight + _radius;
	extents.z = std::abs(up.z) * halfCylHeight + _radius;

	//AABBﾌ最粛ﾆ最鼠XV
	_aabb.min = _owner->_transform._position - extents;
	_aabb.max = _owner->_transform._position + extents;

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

	KTVECTOR3 s1 = capsuleA->_transform._position - upA * (cylinderHeightA * 0.5f);
	KTVECTOR3 e1 = capsuleA->_transform._position + upA * (cylinderHeightA * 0.5f);

	KTVECTOR3 s2 = capsuleB->_transform._position - upB * (cylinderHeightB * 0.5f);
	KTVECTOR3 e2 = capsuleB->_transform._position + upB * (cylinderHeightB * 0.5f);

	//ﾅ近傍_2_ﾟ・
	KTVECTOR3 closestPointA, closestPointB;
	ClosestPointSegSeg(s1, e1, s2, e2, closestPointA, closestPointB);

	KTVECTOR3 diff = closestPointB - closestPointA;
	float distSq = diff.MagnitudeSqr();
	float radiusSum = this->_radius + other->_radius;

	if (distSq < radiusSum * radiusSum) {//ﾕ・
		outCollisionManifold.hasCollision = true;
		outCollisionManifold.a = const_cast<ColliderCapsule*>(other);
		outCollisionManifold.b = const_cast<ColliderCapsule*>(this);

		float dist = diff.Magnitude();
		outCollisionManifold.penetrationDepth = radiusSum - dist;
		if (dist > 1e-5f)
		{
			outCollisionManifold.normal = diff.Normalize();
		}
		else {//Sﾉ重ﾈてれ合
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
	//JvZﾌ椴始_(A)ﾆ終_(B)ﾟ・
	float cylinderHeight = (std::max)(0.0f, this->_height - 2.0f * this->_radius);
	GameObject* capsuleObj = this->GetOwner();
	KTVECTOR3 up = capsuleObj->GetUp();

	KTVECTOR3 A = capsuleObj->_transform._position - up * (cylinderHeight * 0.5f);
	KTVECTOR3 B = capsuleObj->_transform._position + up * (cylinderHeight * 0.5f);

	//ﾌ担_(C)
	KTVECTOR3 C = other->GetOwner()->_transform._position;

	//_CABﾌ最近接点(P)vZ
	KTVECTOR3 AB = B - A;
	KTVECTOR3 AC = C - A;

	float t = 0.0f;
	float abLengthSqr = AB.MagnitudeSqr();

	//_ﾉ退夏なＡFbNi[Zh~j
	if (abLengthSqr > 1e-6f) {
		t = Dot(AC, AB) / abLengthSqr;
	}

	//t0.0`1.0ﾉクvﾄ惜・
	t = Clamp(t, 0.0f, 1.0f);

	//ﾅ近接点P
	KTVECTOR3 P = A + AB * t;

	//ﾅ近接点Pﾆ粁担Cﾌ暁衝突・
	KTVECTOR3 diff = C - P; // PCﾖのベNg
	float distSq = diff.MagnitudeSqr();
	float radiusSum = this->_radius + other->_radius;

	if (distSq <= radiusSum * radiusSum) {//ﾕ突て・
		outCollisionManifold.hasCollision = true;

		outCollisionManifold.a = const_cast<ColliderSphere*>(other);
		outCollisionManifold.b = const_cast<ColliderCapsule*>(this);

		float dist = diff.Magnitude();
		outCollisionManifold.penetrationDepth = radiusSum - dist;

		//@ﾌ計Z (JvZ迢・頴)
		if (dist > 1e-5f) {
			outCollisionManifold.normal = diff.Normalize(); // K
		}
		else {
			// Sﾉ担dﾈてれ合ﾍ適ﾈ碧頴
			outCollisionManifold.normal = KTVECTOR3(0.0f, 1.0f, 0.0f);
		}

		// ﾚ触_ﾌ計Z (ﾌ表ﾊ就ﾜは胆)
		ContactPoint cp;
		cp.position = C - outCollisionManifold.normal * other->_radius;
		cp.penetration = outCollisionManifold.penetrationDepth;
		outCollisionManifold.contacts.push_back(cp);

		return true;
	}

	return false;
}

bool ColliderCapsule::CheckVSOBB(const ColliderBox* other, CollisionManifold& outCollisionManifold)const {
	//JvZﾌ椴始_(A)ﾆ終_(B)[hﾔで計Z
	float cylinderHeight = (std::max)(0.0f, this->_height - 2.0f * this->_radius);
	KTVECTOR3 up = this->GetOwner()->GetUp();
	KTVECTOR3 A = this->GetOwner()->_transform._position - up * (cylinderHeight * 0.5f);
	KTVECTOR3 B = this->GetOwner()->_transform._position + up * (cylinderHeight * 0.5f);

	//OBBﾌゼJﾔ（AABBﾆて慰）ABﾏ・
	KTVECTOR3 boxPos = other->GetOwner()->_transform._position;
	KTVECTOR3 dA = A - boxPos;
	KTVECTOR3 dB = B - boxPos;

	//BoxﾌゼJgﾄ兎i]ﾌ逆ﾏ核
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

	//ﾝ射影@ (Alternating Projection) ﾅ最近接点T
	KTVECTOR3 P = (localA + localB) * 0.5f; // ﾌ点P (lﾍ胆)
	KTVECTOR3 Q;							// OBBﾌ点Q
	KTVECTOR3 ab = localB - localA;
	float abLenSq = ab.MagnitudeSqr();

	//3求[vﾎ姿ﾍ慨ﾉ・
	for (int i = 0; i < 3; ++i) {
		// 闖②: PAABBﾉクvQﾟ・
		Q.x = Clamp(P.x, -other->_extents.x, other->_extents.x);
		Q.y = Clamp(P.y, -other->_extents.y, other->_extents.y);
		Q.z = Clamp(P.z, -other->_extents.z, other->_extents.z);

		// 闖③: QABﾖの最近接点ﾟ１XV
		if (abLenSq > 1e-6f) {
			float t = Dot(Q - localA, ab) / abLenSq;
			t = Clamp(t, 0.0f, 1.0f);
			P = localA + ab * t;
		}
		else {
			P = localA;
		}
	}

	//ﾅ訓ﾅ終IQm
	Q.x = Clamp(P.x, -other->_extents.x, other->_extents.x);
	Q.y = Clamp(P.y, -other->_extents.y, other->_extents.y);
	Q.z = Clamp(P.z, -other->_extents.z, other->_extents.z);

	//ﾕ突犯マjtH[hﾌ構z
	KTVECTOR3 PQ = Q - P; // P(Capsule)Q(Box)ﾖのベNg
	float distSq = PQ.MagnitudeSqr();

	if (distSq <= this->_radius * this->_radius) {
		outCollisionManifold.hasCollision = true;
		outCollisionManifold.a = const_cast<ColliderBox*>(other);
		outCollisionManifold.b = const_cast<ColliderCapsule*>(this);

		KTVECTOR3 normalLocal;
		float dist = 0.0f;

		if (distSq > 1e-6f) {
			// ﾊ称接触
			dist = sqrtf(distSq);
			normalLocal = PQ / dist; // K
		}
		else {
			// SBoxﾌ橡めり込ﾅれ合iPQvj
			// ﾅめ・oxﾌ面５ﾄ、ﾖ頴@
			float minDist = FLT_MAX;

			float dx = other->_extents.x - fabs(P.x);
			if (dx < minDist) { minDist = dx; normalLocal = KTVECTOR3((P.x > 0) ? 1 : -1, 0, 0); }

			float dy = other->_extents.y - fabs(P.y);
			if (dy < minDist) { minDist = dy; normalLocal = KTVECTOR3(0, (P.y > 0) ? 1 : -1, 0); }

			float dz = other->_extents.z - fabs(P.z);
			if (dz < minDist) { minDist = dz; normalLocal = KTVECTOR3(0, 0, (P.z > 0) ? 1 : -1); }

			dist = -minDist; // ﾟり込ﾅので僅マCiXﾆて・
		}

		outCollisionManifold.penetrationDepth = this->_radius - dist;

		// [J@[hﾔの法ﾉ変・
		outCollisionManifold.normal =
			other->_axis[0] * normalLocal.x +
			other->_axis[1] * normalLocal.y +
			other->_axis[2] * normalLocal.z;

		// ﾚ触_ﾌ計Z ([JQ[hﾔに戻・
		KTVECTOR3 Q_world = boxPos +
			other->_axis[0] * Q.x +
			other->_axis[1] * Q.y +
			other->_axis[2] * Q.z;

		ContactPoint cp;
		cp.position = Q_world; // Boxﾌ表ﾊぼ触_ﾆ・
		cp.penetration = outCollisionManifold.penetrationDepth;
		outCollisionManifold.contacts.push_back(cp);

		return true;
	}

	return false;
}

KTMATRIX3 ColliderCapsule::ComputeLocalInertiaTensor(float mass){
	//~ﾌ・
	float cylinderHeight = (std::max)(0.0f, _height - 2.0f * _radius);

	//aAﾜは写・ﾈ歌場合ﾍゼsﾔ・
	if (_radius <= 0.001f || mass <= 0.0f) {
		return KTMATRIX3::Zero();
	}

	const float PI = 3.14159265359f;

	//ﾌ積の計Z
	float volumeCylinder = PI * _radius * _radius * cylinderHeight;
	float volumeSphere = (4.0f / 3.0f) * PI * _radius * _radius * _radius;
	float volumeTotal = volumeCylinder + volumeSphere;

	if (volumeTotal == 0.0f) return KTMATRIX3::Zero();

	//ﾊの布
	float massCylinder = mass * (volumeCylinder / volumeTotal);
	float massSphere = mass * (volumeSphere / volumeTotal);

	//Yijﾜの骸g
	float iy = (0.5f * massCylinder * _radius * _radius) +
		(0.4f * massSphere * _radius * _radius);

	//XEZiZjﾜの骸g
	//~ X/Z 
	float ixzCylinder = (1.0f / 12.0f) * massCylinder * (3.0f * _radius * _radius + cylinderHeight * cylinderHeight);

	// X/Z isﾌ定理Kpﾏみ）
	float ixzSphere = massSphere * (0.4f * _radius * _radius +
		0.5f * cylinderHeight * cylinderHeight +
		0.375f * cylinderHeight * _radius);

	float ix = ixzCylinder + ixzSphere;
	float iz = ix;

	//e\sﾉて返・
	return KTMATRIX3(
		ix, 0.0f, 0.0f,
		0.0f, iy, 0.0f,
		0.0f, 0.0f, iz
	);
}

void ColliderCapsule::ShowUI(){
	ImGui::Checkbox("_wasOverlap", &_wasOverlap);
}
