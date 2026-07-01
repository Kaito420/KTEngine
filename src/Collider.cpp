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
	float s, t;//}ϐ

	KTVECTOR3 d1 = e1 - s1;	//̕xNg
	KTVECTOR3 d2 = e2 - s2;
	KTVECTOR3 r = s1 - s2;	//n_m̃xNgi21j

	float lenSqD1 = d1.MagnitudeSqr();//̒2
	float lenSqD2 = d2.MagnitudeSqr();
	float f = Dot(d2, r);

	const float EPSILON = 1e-6f;

	if (lenSqD1 <= EPSILON && lenSqD2 <= EPSILON) {//_ɑމĂꍇ
		s = t = 0.0f;
		c1 = s1;
		c2 = s2;
		return;
	}
	if (lenSqD1 <= EPSILON) {	//1_ɑމĂꍇ
		s = 0.0f;
		t = Clamp(f / lenSqD2, 0.0f, 1.0f);
	}
	else {	//1͓_Ȃ
		float c = Dot(d1, r);
		if (lenSqD2 <= EPSILON) {	//2_ɑމ
			t = 0.0f;
			s = Clamp(-c / lenSqD1, 0.0f, 1.0f);
		}
		else {	//ǂ̂܂
			float b = Dot(d1, d2);	//̕xNgm̓
			float denom = lenSqD1 * lenSqD2 - b * b;	//Ň̕

			if (denom != 0.0f) {//słȂꍇ
				s = Clamp((b * f - c * lenSqD2) / denom, 0.0f, 1.0f);
			}
			else {//͕s
				s = 0.0f;
			}
			t = (b * s + f) / lenSqD2;

			//t0~1oꍇ̍ČvZ
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
}

void ColliderSphere::Start(){
	Manager::GetCurrentScene()->GetPhysicsSystem()->RegisterCollider(this);
}

void ColliderSphere::OnDestroy(){
	Manager::GetCurrentScene()->GetPhysicsSystem()->RemoveCollider(this);
}

void ColliderSphere::Update(){
	//_owner->_transform._scalëԑ傫l𔽉f
	float tempScale = _owner->_transform._scale.x;
	if (tempScale < _owner->_transform._scale.y) {
		tempScale = _owner->_transform._scale.y;
	}
	if (tempScale < _owner->_transform._scale.z) {
		tempScale = _owner->_transform._scale.z;
	}
	//ځiSphereMesh甼a𔽉fj
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

void ColliderSphere::Render() const{

}

bool ColliderSphere::CheckVSSphere(const ColliderSphere* other, CollisionManifold& outCollisionManifold) const{

	if(!this->_aabb.CheckOverlap(other->_aabb))
		return false;

	outCollisionManifold.a = const_cast<ColliderSphere*>(other);
	outCollisionManifold.b = const_cast<ColliderSphere*>(this);

	//SԂ̕
	float distanceSqr = (this->_owner->_transform._position -
		other->_owner->_transform._position).MagnitudeSqr();

	//a̘a̕
	float radiusSumSqr = (this->_radius + other->_radius) * (this->_radius + other->_radius);

	if (distanceSqr <= radiusSumSqr) {//Ă
		outCollisionManifold.penetrationDepth = sqrtf(radiusSumSqr) - sqrtf(distanceSqr);
		outCollisionManifold.normal = (other->_owner->_transform._position -
			this->_owner->_transform._position).Normalize();
		KTVECTOR3 cpa;//A̐ڐG_
		cpa = other->_owner->_transform._position -
			_radius * outCollisionManifold.normal;

		KTVECTOR3 cpb;//B̐ڐG_
		cpb = this->_owner->_transform._position +
			_radius * outCollisionManifold.normal;

		ContactPoint cp;//ڐG_m̒_Փˉɗp
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

	KTVECTOR3 bPos = other->GetOwner()->_transform._position;	//OBB̍W
	KTVECTOR3 cPos = this->_owner->_transform._position;		//Sphere̍W

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

	float distanceSqr = (closestPoint - cPos).MagnitudeSqr();
	float radiusSqr = this->_radius * this->_radius;

	if (distanceSqr <= radiusSqr) {
		outCollisionManifold.penetrationDepth = sqrtf(radiusSqr) - sqrtf(distanceSqr);
		outCollisionManifold.normal = (closestPoint - cPos).Normalize();
		ContactPoint cp;
		cp.penetration = sqrtf(radiusSqr) - sqrtf(distanceSqr);
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

	_vertexBuffer = Renderer::CreateVertexBuffer(sizeof(Vertex), 8);

	Vertex vertex[8] = {
		{ { -_extents.x, +_extents.y, +_extents.z},{0,0,0},{0,1,0,1},{0,0} },
		{ { +_extents.x, +_extents.y, +_extents.z},{0,0,0},{0,1,0,1},{0,0} },
		{ { +_extents.x, +_extents.y, -_extents.z},{0,0,0},{0,1,0,1},{0,0} },
		{ { -_extents.x, +_extents.y, -_extents.z},{0,0,0},{0,1,0,1},{0,0} },
		{ { -_extents.x, -_extents.y, +_extents.z},{0,0,0},{0,1,0,1},{0,0} },
		{ { +_extents.x, -_extents.y, +_extents.z},{0,0,0},{0,1,0,1},{0,0} },
		{ { +_extents.x, -_extents.y, -_extents.z},{0,0,0},{0,1,0,1},{0,0} },
		{ { -_extents.x, -_extents.y, -_extents.z},{0,0,0},{0,1,0,1},{0,0} }
	};

	void* data = nullptr;
	HRESULT hr = _vertexBuffer->Resource->Map(0, nullptr, &data);
	if (SUCCEEDED(hr)) {
		memcpy(data, vertex, sizeof(vertex));
		_vertexBuffer->Resource->Unmap(0, nullptr);
	}

	_indexBuffer = Renderer::CreateIndexBuffer(24);

	unsigned int indices[] = {
		0,1, 1,2, 2,3, 3,0,
		4,5, 5,6, 6,7, 7,4,
		0,4, 1,5, 2,6, 3,7
	};

	hr = _indexBuffer->Resource->Map(0, nullptr, &data);
	if (SUCCEEDED(hr)) {
		memcpy(data, indices, sizeof(indices));
		_indexBuffer->Resource->Unmap(0, nullptr);
	}
}

void ColliderBox::Start(){
	Manager::GetCurrentScene()->GetPhysicsSystem()->RegisterCollider(this);
}

void ColliderBox::OnDestroy(){
	Manager::GetCurrentScene()->GetPhysicsSystem()->RemoveCollider(this);
}

void ColliderBox::Update() {
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

void ColliderBox::Render() const {
	Vertex vertex[8] = {
		{ { -_extents.x, +_extents.y, +_extents.z},{0,0,0},{0,1,0,1},{0,0} },
		{ { +_extents.x, +_extents.y, +_extents.z},{0,0,0},{0,1,0,1},{0,0} },
		{ { +_extents.x, +_extents.y, -_extents.z},{0,0,0},{0,1,0,1},{0,0} },
		{ { -_extents.x, +_extents.y, -_extents.z},{0,0,0},{0,1,0,1},{0,0} },
		{ { -_extents.x, -_extents.y, +_extents.z},{0,0,0},{0,1,0,1},{0,0} },
		{ { +_extents.x, -_extents.y, +_extents.z},{0,0,0},{0,1,0,1},{0,0} },
		{ { +_extents.x, -_extents.y, -_extents.z},{0,0,0},{0,1,0,1},{0,0} },
		{ { -_extents.x, -_extents.y, -_extents.z},{0,0,0},{0,1,0,1},{0,0} }
	};

	void* data = nullptr;
	HRESULT hr = _vertexBuffer->Resource->Map(0, nullptr, &data);
	if (SUCCEEDED(hr)) {
		memcpy(data, vertex, sizeof(vertex));
		_vertexBuffer->Resource->Unmap(0, nullptr);
	}

	auto cmdList = Renderer::GetCommandListDX12();
	if (!cmdList) return;

	D3D12_VERTEX_BUFFER_VIEW vbView = {};
	vbView.BufferLocation = _vertexBuffer->Resource->GetGPUVirtualAddress();
	vbView.StrideInBytes = _vertexBuffer->Stride;
	vbView.SizeInBytes = _vertexBuffer->Stride * _vertexBuffer->Size;
	cmdList->IASetVertexBuffers(0, 1, &vbView);

	D3D12_INDEX_BUFFER_VIEW ibView = {};
	ibView.BufferLocation = _indexBuffer->Resource->GetGPUVirtualAddress();
	ibView.SizeInBytes = sizeof(unsigned int) * _indexBuffer->Size;
	ibView.Format = DXGI_FORMAT_R32_UINT;
	cmdList->IASetIndexBuffer(&ibView);

	XMMATRIX translation = XMMatrixTranslation(_owner->_transform._position.x, _owner->_transform._position.y, _owner->_transform._position.z);
	XMFLOAT4 q = XMFLOAT4(_owner->_transform._quaternion.x, _owner->_transform._quaternion.y, _owner->_transform._quaternion.z, _owner->_transform._quaternion.w);
	XMMATRIX rotation = XMMatrixRotationQuaternion(XMLoadFloat4(&q));
	XMMATRIX worldMatrix = rotation * translation;

	Renderer::SetWorldMatrix(worldMatrix);

	MATERIAL material = {};
	material.Diffuse = { 1.0f, 1.0f, 1.0f, 1.0f };
	material.TextureEnable = false;
	Renderer::SetConstant(3, &material, sizeof(material));

	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
	// PSOバインド
	{
		std::string vsId = "UnlitColorVS";
		std::string psId = "UnlitColorPS";
		auto shaderComp = _owner->GetComponent<Shader>();
		if (shaderComp) {
			vsId = shaderComp->GetVertexShaderID();
			psId = shaderComp->GetPixelShaderID();
		}
		ID3D12PipelineState* pso = ShaderManager::Instance().GetPipelineState(vsId, psId, 1, Renderer::GetCullModeDX12(), Renderer::GetDepthEnableDX12(), Renderer::GetDepthWriteDX12(), D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE);
		cmdList->SetPipelineState(pso);
	}

		Renderer::BindShaderConstantsDX12();
cmdList->DrawIndexedInstanced(24, 1, 0, 0, 0);
}

bool ColliderBox::CheckVSOBB(const ColliderBox* other, CollisionManifold& manifold) const {

	if (!this->_aabb.CheckOverlap(other->_aabb))
		return false;

	manifold.a = const_cast<ColliderBox*>(other);
	manifold.b = const_cast<ColliderBox*>(this);

	float minOverlap = FLT_MAX;	//ŏ̏dȂ
	KTVECTOR3 bestAxis;			//ŏ̏dȂ莲

	//eɂēeă`FbN
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

	//Oςłł9mF
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

	//ՓˏXV

	KTVECTOR3 centerDelta = other->_center - _center;
	if (Dot(centerDelta, bestAxis) < 0.0f)
		bestAxis = -bestAxis;

	manifold.hasCollision = true;
	manifold.normal = bestAxis.Normalize();
	manifold.penetrationDepth = minOverlap;


	//Փ˓_̌vZ
	FixedList<KTVECTOR3, 16> contactPolygon = ComputeContactPolygon(this, other, manifold.normal);
	if (!contactPolygon.empty()) {
		for (auto& p : contactPolygon) {
			ContactPoint cp;
			cp.position = p;
			cp.penetration = minOverlap;	//Ƃ肠lA萳mɂȂʂɎZo
			manifold.contacts.push_back(cp);
		}
	}
	else {//Nbvŏꍇ̏iŋߖT_j
		KTVECTOR3 pointOnA = _center + manifold.normal * (_extents.x + _extents.y + _extents.z);
		KTVECTOR3 pointOnB = other->_center - manifold.normal * (other->_extents.x + other->_extents.y + other->_extents.z);
		ContactPoint cp;
		cp.position = (pointOnA + pointOnB) * 0.5f;
		cp.penetration = minOverlap;
		manifold.contacts.push_back(cp);
	}
	
	//SĂ̎ŏdȂĂ̂ŏՓ˂Ă
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

	//g̓e
	float centerA = Dot(_center, L);
	float rA = _extents.x * fabs(Dot(_axis[0], L)) +
		_extents.y * fabs(Dot(_axis[1], L)) +
		_extents.z * fabs(Dot(_axis[2], L));

	//̓e
	float centerB = Dot(other->_center, L);
	float rB = other->_extents.x * fabs(Dot(other->_axis[0], L)) +
		other->_extents.y * fabs(Dot(other->_axis[1], L)) +
		other->_extents.z * fabs(Dot(other->_axis[2], L));

	//S
	float distance = fabs(centerA - centerB);

	//̗L
	return distance <= (rA + rB);

}

bool ColliderBox::OverlapOnAxis(const ColliderBox* other, const KTVECTOR3& axis, float& outOverlap) const
{
	if (axis.Magnitude() < 1e-6f) {
		outOverlap = FLT_MAX;
		return true;
	}

	KTVECTOR3 L = axis.Normalize();

	//g̓e
	float centerA = Dot(_center, L);
	float rA = _extents.x * fabs(Dot(_axis[0], L)) +
		_extents.y * fabs(Dot(_axis[1], L)) +
		_extents.z * fabs(Dot(_axis[2], L));

	//̓e
	float centerB = Dot(other->_center, L);
	float rB = other->_extents.x * fabs(Dot(other->_axis[0], L)) +
		other->_extents.y * fabs(Dot(other->_axis[1], L)) +
		other->_extents.z * fabs(Dot(other->_axis[2], L));

	//S
	float distance = fabs(centerA - centerB);
	outOverlap = (rA + rB) - distance;

	//̗L
	return distance <= (rA + rB);
}

FixedList<KTVECTOR3, 4> ColliderBox::GetFaceVertices(const ColliderBox* box, int axisIndex, int sign){
	FixedList<KTVECTOR3, 4> verts;

	// extents ̊eo
	float ex = box->_extents.x;
	float ey = box->_extents.y;
	float ez = box->_extents.z;

	float extent = (axisIndex == 0) ? ex : (axisIndex == 1 ? ey : ez);

	// face center
	KTVECTOR3 faceCenter = box->_center + box->_axis[axisIndex] * (extent * (float)sign);

	// face ̃[J2
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
			//  -> B ǉ
			out.push_back(B);
		}
		else if (inA && !inB) {
			// A inside, B outside -> _ǉ
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
			// O -> Ȃ
		}
	}

	return out;
}


FixedList<KTVECTOR3, 16> ColliderBox::ComputeContactPolygon(const ColliderBox* refBox, const ColliderBox* incBox, const KTVECTOR3& collisionNormal){
	//Qƃ{bNXirefBoxjюQƖʂ̌
	//QƎ bestAxis ɍł߂ (abs dot ő) I
	int refAxis = 0;
	float bestDot = fabs(Dot(refBox->_axis[0], collisionNormal));
	for (int i = 1; i < 3; ++i) {
		float d = fabs(Dot(refBox->_axis[i], collisionNormal));
		if (d > bestDot) { bestDot = d; refAxis = i; }
	}

	//@ƎAt
	float s = Dot(collisionNormal, refBox->_axis[refAxis]);
	int refSign = (s >= 0.0f) ? +1 : -1;

	//|S = QƖʂ4_
	FixedList<KTVECTOR3, 16> poly;
	auto initalVerts =  GetFaceVertices(refBox, refAxis, refSign);

	//^킹邽߂ɃRs[
	for(const auto& v : initalVerts){
		poly.push_back(v);
	}

	//CVfg{bNX(incBox)6ʂ擾
	FixedList<Plane, 8> incPlanes = GetOBBPlanes(incBox);

	//polyeʂŏɃNbv
	for (const Plane& pl : incPlanes) {
		poly = ClipPolygonAgainstPlane(poly, pl);
		if (poly.empty()) break;
	}

	return poly; // ȂNbvŏƂ
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
}

void ColliderCapsule::Start() {
	Manager::GetCurrentScene()->GetPhysicsSystem()->RegisterCollider(this);
}

void ColliderCapsule::OnDestroy() {
	Manager::GetCurrentScene()->GetPhysicsSystem()->RemoveCollider(this);
}

void ColliderCapsule::Update() {
	//GameObject̏ōXV
	// _owner->_transform._scale ̈ԑ傫l𔽉fic݂h߂̋ϓXP[j
	float tempScaleRad = _owner->_transform._scale.x;
	float tempScaleHeight = _owner->_transform._scale.y;
	if (tempScaleRad < _owner->_transform._scale.z) tempScaleRad = _owner->_transform._scale.z;

	// ځiCapsuleR|[lgjp[^𔽉f
	Capsule* cap = _owner->GetComponent<Capsule>();
	if (cap) {
		_radius = cap->_radius * tempScaleRad;
		_height = cap->_height * tempScaleHeight;
	}
	else {
		_radius = tempScaleRad * 0.5f;
		_height = tempScaleHeight * 2.0f;
	}

	//AABB̍XV
	float halfCylHeight = (std::max)(0.0f, _height - 2.0f * _radius) * 0.5f;

	//JvŽ݂YiUpxNgj擾
	KTVECTOR3 up = _owner->GetUp();

	//eiX, Y, Zjɑ΂ĂǂꂾLтĂ邩vZAa𑫂
	KTVECTOR3 extents;
	extents.x = std::abs(up.x) * halfCylHeight + _radius;
	extents.y = std::abs(up.y) * halfCylHeight + _radius;
	extents.z = std::abs(up.z) * halfCylHeight + _radius;

	//AABB̍ŏlƍőlXV
	_aabb.min = _owner->_transform._position - extents;
	_aabb.max = _owner->_transform._position + extents;
}

void ColliderCapsule::Render() const{
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

	//ŋߖT_2_߂
	KTVECTOR3 closestPointA, closestPointB;
	ClosestPointSegSeg(s1, e1, s2, e2, closestPointA, closestPointB);

	KTVECTOR3 diff = closestPointB - closestPointA;
	float distSq = diff.MagnitudeSqr();
	float radiusSum = this->_radius + other->_radius;

	if (distSq < radiusSum * radiusSum) {//Փ
		outCollisionManifold.hasCollision = true;
		outCollisionManifold.a = const_cast<ColliderCapsule*>(other);
		outCollisionManifold.b = const_cast<ColliderCapsule*>(this);

		float dist = diff.Magnitude();
		outCollisionManifold.penetrationDepth = radiusSum - dist;
		if (dist > 1e-5f)
		{
			outCollisionManifold.normal = diff.Normalize();
		}
		else {//SɏdȂĂꍇ
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
	//JvZ̓̎n_(A)ƏI_(B)߂
	float cylinderHeight = (std::max)(0.0f, this->_height - 2.0f * this->_radius);
	GameObject* capsuleObj = this->GetOwner();
	KTVECTOR3 up = capsuleObj->GetUp();

	KTVECTOR3 A = capsuleObj->_transform._position - up * (cylinderHeight * 0.5f);
	KTVECTOR3 B = capsuleObj->_transform._position + up * (cylinderHeight * 0.5f);

	//̒S_(C)
	KTVECTOR3 C = other->GetOwner()->_transform._position;

	//_CAB̍ŋߐړ_(P)vZ
	KTVECTOR3 AB = B - A;
	KTVECTOR3 AC = C - A;

	float t = 0.0f;
	float abLengthSqr = AB.MagnitudeSqr();

	//_ɑމĂȂ`FbNi[Zh~j
	if (abLengthSqr > 1e-6f) {
		t = Dot(AC, AB) / abLengthSqr;
	}

	//t0.0`1.0ɃNvĐɐ
	t = Clamp(t, 0.0f, 1.0f);

	//ŋߐړ_P
	KTVECTOR3 P = A + AB * t;

	//ŋߐړ_PƋ̒SC̋ŏՓ˔
	KTVECTOR3 diff = C - P; // PCւ̃xNg
	float distSq = diff.MagnitudeSqr();
	float radiusSum = this->_radius + other->_radius;

	if (distSq <= radiusSum * radiusSum) {//Փ˂Ă
		outCollisionManifold.hasCollision = true;

		outCollisionManifold.a = const_cast<ColliderSphere*>(other);
		outCollisionManifold.b = const_cast<ColliderCapsule*>(this);

		float dist = diff.Magnitude();
		outCollisionManifold.penetrationDepth = radiusSum - dist;

		//@̌vZ (JvZ狅։o)
		if (dist > 1e-5f) {
			outCollisionManifold.normal = diff.Normalize(); // K
		}
		else {
			// SɒSdȂĂꍇ͓Kȕɉo
			outCollisionManifold.normal = KTVECTOR3(0.0f, 1.0f, 0.0f);
		}

		// ڐG_̌vZ (̕\ʏA܂͒_)
		ContactPoint cp;
		cp.position = C - outCollisionManifold.normal * other->_radius;
		cp.penetration = outCollisionManifold.penetrationDepth;
		outCollisionManifold.contacts.push_back(cp);

		return true;
	}

	return false;
}

bool ColliderCapsule::CheckVSOBB(const ColliderBox* other, CollisionManifold& outCollisionManifold)const {
	//JvZ̓̎n_(A)ƏI_(B)[hԂŌvZ
	float cylinderHeight = (std::max)(0.0f, this->_height - 2.0f * this->_radius);
	KTVECTOR3 up = this->GetOwner()->GetUp();
	KTVECTOR3 A = this->GetOwner()->_transform._position - up * (cylinderHeight * 0.5f);
	KTVECTOR3 B = this->GetOwner()->_transform._position + up * (cylinderHeight * 0.5f);

	//OBB̃[JԁiAABBƂĈԁjABϊ
	KTVECTOR3 boxPos = other->GetOwner()->_transform._position;
	KTVECTOR3 dA = A - boxPos;
	KTVECTOR3 dB = B - boxPos;

	//Box̃[Jgēei]̋tϊj
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

	//ݎˉe@ (Alternating Projection) ōŋߐړ_T
	KTVECTOR3 P = (localA + localB) * 0.5f; // ̓_P (l͒_)
	KTVECTOR3 Q;							// OBB̓_Q
	KTVECTOR3 ab = localB - localA;
	float abLenSq = ab.MagnitudeSqr();

	//3񃋁[vΎp͊SɎ
	for (int i = 0; i < 3; ++i) {
		// 菇A: PAABBɃNvQ߂
		Q.x = Clamp(P.x, -other->_extents.x, other->_extents.x);
		Q.y = Clamp(P.y, -other->_extents.y, other->_extents.y);
		Q.z = Clamp(P.z, -other->_extents.z, other->_extents.z);

		// 菇B: QABւ̍ŋߐړ_߂PXV
		if (abLenSq > 1e-6f) {
			float t = Dot(Q - localA, ab) / abLenSq;
			t = Clamp(t, 0.0f, 1.0f);
			P = localA + ab * t;
		}
		else {
			P = localA;
		}
	}

	//ŌPŏIIQm
	Q.x = Clamp(P.x, -other->_extents.x, other->_extents.x);
	Q.y = Clamp(P.y, -other->_extents.y, other->_extents.y);
	Q.z = Clamp(P.z, -other->_extents.z, other->_extents.z);

	//Փ˔ƃ}jtH[h̍\z
	KTVECTOR3 PQ = Q - P; // P(Capsule)Q(Box)ւ̃xNg
	float distSq = PQ.MagnitudeSqr();

	if (distSq <= this->_radius * this->_radius) {
		outCollisionManifold.hasCollision = true;
		outCollisionManifold.a = const_cast<ColliderBox*>(other);
		outCollisionManifold.b = const_cast<ColliderCapsule*>(this);

		KTVECTOR3 normalLocal;
		float dist = 0.0f;

		if (distSq > 1e-6f) {
			// ʏ̐ڐG
			dist = sqrtf(distSq);
			normalLocal = PQ / dist; // K
		}
		else {
			// SBox̓ɂ߂荞łꍇiPQvj
			// ł߂Box̖ʂTāA։o@
			float minDist = FLT_MAX;

			float dx = other->_extents.x - fabs(P.x);
			if (dx < minDist) { minDist = dx; normalLocal = KTVECTOR3((P.x > 0) ? 1 : -1, 0, 0); }

			float dy = other->_extents.y - fabs(P.y);
			if (dy < minDist) { minDist = dy; normalLocal = KTVECTOR3(0, (P.y > 0) ? 1 : -1, 0); }

			float dz = other->_extents.z - fabs(P.z);
			if (dz < minDist) { minDist = dz; normalLocal = KTVECTOR3(0, 0, (P.z > 0) ? 1 : -1); }

			dist = -minDist; // ߂荞ł̂ŋ̓}CiXƂĈ
		}

		outCollisionManifold.penetrationDepth = this->_radius - dist;

		// [J@[hԂ̖@ɕϊ
		outCollisionManifold.normal =
			other->_axis[0] * normalLocal.x +
			other->_axis[1] * normalLocal.y +
			other->_axis[2] * normalLocal.z;

		// ڐG_̌vZ ([JQ[hԂɖ߂)
		KTVECTOR3 Q_world = boxPos +
			other->_axis[0] * Q.x +
			other->_axis[1] * Q.y +
			other->_axis[2] * Q.z;

		ContactPoint cp;
		cp.position = Q_world; // Box̕\ʂڐG_Ƃ
		cp.penetration = outCollisionManifold.penetrationDepth;
		outCollisionManifold.contacts.push_back(cp);

		return true;
	}

	return false;
}

KTMATRIX3 ColliderCapsule::ComputeLocalInertiaTensor(float mass){
	//~̍
	float cylinderHeight = (std::max)(0.0f, _height - 2.0f * _radius);

	//aA܂͎ʂ0ȉ̏ꍇ̓[sԂ
	if (_radius <= 0.001f || mass <= 0.0f) {
		return KTMATRIX3::Zero();
	}

	const float PI = 3.14159265359f;

	//̐ς̌vZ
	float volumeCylinder = PI * _radius * _radius * cylinderHeight;
	float volumeSphere = (4.0f / 3.0f) * PI * _radius * _radius * _radius;
	float volumeTotal = volumeCylinder + volumeSphere;

	if (volumeTotal == 0.0f) return KTMATRIX3::Zero();

	//ʂ̕z
	float massCylinder = mass * (volumeCylinder / volumeTotal);
	float massSphere = mass * (volumeSphere / volumeTotal);

	//Yij܂̊[g
	float iy = (0.5f * massCylinder * _radius * _radius) +
		(0.4f * massSphere * _radius * _radius);

	//XEZiZj܂̊[g
	//~ X/Z 
	float ixzCylinder = (1.0f / 12.0f) * massCylinder * (3.0f * _radius * _radius + cylinderHeight * cylinderHeight);

	// X/Z is̒藝Kpς݁j
	float ixzSphere = massSphere * (0.4f * _radius * _radius +
		0.5f * cylinderHeight * cylinderHeight +
		0.375f * cylinderHeight * _radius);

	float ix = ixzCylinder + ixzSphere;
	float iz = ix;

	//e\sɂĕԂ
	return KTMATRIX3(
		ix, 0.0f, 0.0f,
		0.0f, iy, 0.0f,
		0.0f, 0.0f, iz
	);
}

void ColliderCapsule::ShowUI(){
	ImGui::Checkbox("_wasOverlap", &_wasOverlap);
}
