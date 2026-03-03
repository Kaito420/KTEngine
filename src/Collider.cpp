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
#include "RendererDX11.h"
#include "Texture.h"

void ClosestPointSegSeg(const KTVECTOR3& s1, const KTVECTOR3& e1, const KTVECTOR3& s2, const KTVECTOR3& e2, KTVECTOR3& c1, KTVECTOR3& c2){
	float s, t;//媒介変数

	KTVECTOR3 d1 = e1 - s1;	//線分の方向ベクトル
	KTVECTOR3 d2 = e2 - s2;
	KTVECTOR3 r = s1 - s2;	//始点同士のベクトル（2→1方向）

	float lenSqD1 = d1.MagnitudeSqr();//線分の長さの2乗
	float lenSqD2 = d2.MagnitudeSqr();
	float f = Dot(d2, r);

	const float EPSILON = 1e-6f;

	if (lenSqD1 <= EPSILON && lenSqD2 <= EPSILON) {//両線分が点に退化している場合
		s = t = 0.0f;
		c1 = s1;
		c2 = s2;
		return;
	}
	if (lenSqD1 <= EPSILON) {	//線分1が点に退化している場合
		s = 0.0f;
		t = Clamp(f / lenSqD2, 0.0f, 1.0f);
	}
	else {	//線分1は点じゃない
		float c = Dot(d1, r);
		if (lenSqD2 <= EPSILON) {	//線分2が点に退化
			t = 0.0f;
			s = Clamp(-c / lenSqD1, 0.0f, 1.0f);
		}
		else {	//どちらも線分のまま
			float b = Dot(d1, d2);	//線分の方向ベクトル同士の内積
			float denom = lenSqD1 * lenSqD2 - b * b;	//クラメルの公式の分母

			if (denom != 0.0f) {//線分が平行でない場合
				s = Clamp((b * f - c * lenSqD2) / denom, 0.0f, 1.0f);
			}
			else {//線分は平行
				s = 0.0f;
			}
			t = (b * s + f) / lenSqD2;

			//tが0~1から出た場合の再計算
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
	//_owner->_transform._scaleの一番大きい値を反映する
	float tempScale = _owner->_transform._scale.x;
	if (tempScale < _owner->_transform._scale.y) {
		tempScale = _owner->_transform._scale.y;
	}
	if (tempScale < _owner->_transform._scale.z) {
		tempScale = _owner->_transform._scale.z;
	}
	//見た目（SphereMeshから半径を反映）
	Sphere* sp;
	if (sp = _owner->GetComponent<Sphere>()) {
		_radius = sp->GetRadius() * tempScale;
	}
	else
		_radius = tempScale * 0.5f;

	//AABB更新
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

	//中心間の平方距離
	float distanceSqr = (this->_owner->_transform._position -
		other->_owner->_transform._position).MagnitudeSqr();

	//半径の和の平方
	float radiusSumSqr = (this->_radius + other->_radius) * (this->_radius + other->_radius);

	if (distanceSqr <= radiusSumSqr) {//当たっている
		outCollisionManifold.penetrationDepth = sqrtf(radiusSumSqr) - sqrtf(distanceSqr);
		outCollisionManifold.normal = (other->_owner->_transform._position -
			this->_owner->_transform._position).Normalize();
		KTVECTOR3 cpa;//A側の接触点
		cpa = other->_owner->_transform._position -
			_radius * outCollisionManifold.normal;

		KTVECTOR3 cpb;//B側の接触点
		cpb = this->_owner->_transform._position +
			_radius * outCollisionManifold.normal;

		ContactPoint cp;//接触点同士の中点を衝突解消に利用する
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

	KTVECTOR3 bPos = other->GetOwner()->_transform._position;	//OBBの座標
	KTVECTOR3 cPos = this->_owner->_transform._position;		//Sphereの座標

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

	D3D11_BUFFER_DESC bd = {};
	bd.Usage = D3D11_USAGE_DYNAMIC;
	bd.ByteWidth = sizeof(Vertex) * 8;
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	RendererDX11::GetDevice()->CreateBuffer(&bd, NULL, _vertexBuffer.GetAddressOf());

	D3D11_MAPPED_SUBRESOURCE msr;
	RendererDX11::GetContext()->Map(_vertexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &msr);
	Vertex* vertex = (Vertex*)msr.pData;

	//デバッグ用ワイヤーフレーム頂点バッファ生成
	vertex[0] = { { -_extents.x, +_extents.y, +_extents.z},{0,0,0},{0,1,0,1},{0,0} };	//0　上段左上
	vertex[1] = { { +_extents.x, +_extents.y, +_extents.z},{0,0,0},{0,1,0,1},{0,0} };	//1　上段右上
	vertex[2] = { { +_extents.x, +_extents.y, -_extents.z},{0,0,0},{0,1,0,1},{0,0} };	//2　上段右下
	vertex[3] = { { -_extents.x, +_extents.y, -_extents.z},{0,0,0},{0,1,0,1},{0,0} };	//3　上段左下
	vertex[4] = { { -_extents.x, -_extents.y, +_extents.z},{0,0,0},{0,1,0,1},{0,0} };	//4　下段左上
	vertex[5] = { { +_extents.x, -_extents.y, +_extents.z},{0,0,0},{0,1,0,1},{0,0} };	//5　下段右上
	vertex[6] = { { +_extents.x, -_extents.y, -_extents.z},{0,0,0},{0,1,0,1},{0,0} };	//6　下段右下
	vertex[7] = { { -_extents.x, -_extents.y, -_extents.z},{0,0,0},{0,1,0,1},{0,0} };	//7　下段左下

	RendererDX11::GetContext()->Unmap(_vertexBuffer.Get(), 0);

	//インデックスバッファ生成
	unsigned short indices[] =
	{
		//上面
		0,1,
		1,2,
		2,3,
		3,0,
		//下面
		4,5,
		5,6,
		6,7,
		7,4,
		//側面
		0,4,
		1,5,
		2,6,
		3,7
	};

	D3D11_BUFFER_DESC ibd = {};
	ibd.Usage = D3D11_USAGE_DEFAULT;
	ibd.ByteWidth = sizeof(unsigned short) * _countof(indices);
	ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
	ibd.CPUAccessFlags = 0;

	D3D11_SUBRESOURCE_DATA data = {};
	data.pSysMem = indices;

	RendererDX11::GetDevice()->CreateBuffer(&ibd, &data, _indexBuffer.GetAddressOf());

}

void ColliderBox::Start(){
	Manager::GetCurrentScene()->GetPhysicsSystem()->RegisterCollider(this);
}

void ColliderBox::OnDestroy(){
	Manager::GetCurrentScene()->GetPhysicsSystem()->RemoveCollider(this);
}

void ColliderBox::Update() {
	//ローカル軸更新
	_center = _owner->_transform._position;
	_axis[0] = _owner->GetRight();
	_axis[1] = _owner->GetUp();
	_axis[2] = _owner->GetForward();

	_extents = _owner->_transform._scale * 0.5f;

	//AABB更新
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

void ColliderBox::Render()const {

	D3D11_MAPPED_SUBRESOURCE msr;
	RendererDX11::GetContext()->Map(_vertexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &msr);
	Vertex* vertex = (Vertex*)msr.pData;

	//デバッグ用ワイヤーフレーム頂点バッファ生成
	vertex[0] = { { - _extents.x, + _extents.y, + _extents.z},{0,0,0},{0,1,0,1},{0,0} };	//0　上段左上
	vertex[1] = { { + _extents.x, + _extents.y, + _extents.z},{0,0,0},{0,1,0,1},{0,0} };	//1　上段右上
	vertex[2] = { { + _extents.x, + _extents.y, - _extents.z},{0,0,0},{0,1,0,1},{0,0} };	//2　上段右下
	vertex[3] = { { - _extents.x, + _extents.y, - _extents.z},{0,0,0},{0,1,0,1},{0,0} };	//3　上段左下
	vertex[4] = { { - _extents.x, - _extents.y, + _extents.z},{0,0,0},{0,1,0,1},{0,0} };	//4　下段左上
	vertex[5] = { { + _extents.x, - _extents.y, + _extents.z},{0,0,0},{0,1,0,1},{0,0} };	//5　下段右上
	vertex[6] = { { + _extents.x, - _extents.y, - _extents.z},{0,0,0},{0,1,0,1},{0,0} };	//6　下段右下
	vertex[7] = { { - _extents.x, - _extents.y, - _extents.z},{0,0,0},{0,1,0,1},{0,0} };	//7　下段左下

	RendererDX11::GetContext()->Unmap(_vertexBuffer.Get(), 0);

	//頂点バッファ設定
	UINT stride = sizeof(Vertex);
	UINT offset = 0;
	RendererDX11::GetContext()->IASetVertexBuffers(0, 1, _vertexBuffer.GetAddressOf(), &stride, &offset);
	RendererDX11::GetContext()->IASetIndexBuffer(_indexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);
	//マトリクス設定
	XMMATRIX translation = XMMatrixTranslation(_owner->_transform._position.x, _owner->_transform._position.y, _owner->_transform._position.z);

	XMFLOAT4 q = XMFLOAT4(_owner->_transform._quaternion.x, _owner->_transform._quaternion.y, _owner->_transform._quaternion.z, _owner->_transform._quaternion.w);

	XMMATRIX rotation = XMMatrixRotationQuaternion(XMLoadFloat4(&q));

	XMMATRIX scale = XMMatrixScaling(_owner->_transform._scale.x, _owner->_transform._scale.y, _owner->_transform._scale.z);

	XMMATRIX worldMatrix = rotation * translation;

	RendererDX11::SetWorldMatrix(worldMatrix);

	//プリミティブトポロジ設定
	RendererDX11::GetContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);

	MATERIAL material = {};
	material.Diffuse = { 1.0f, 1.0f, 1.0f, 1.0f };
	material.TextureEnable = false;
	RendererDX11::SetMaterial(material);

	//ポリゴン描画
	RendererDX11::GetContext()->DrawIndexed(24, 0, 0);

}

bool ColliderBox::CheckVSOBB(const ColliderBox* other, CollisionManifold& manifold) const {

	if (!this->_aabb.CheckOverlap(other->_aabb))
		return false;

	manifold.a = const_cast<ColliderBox*>(other);
	manifold.b = const_cast<ColliderBox*>(this);

	float minOverlap = FLT_MAX;	//最小の重なり量
	KTVECTOR3 bestAxis;			//最小の重なり軸

	//各軸について投影してチェック
	//分離軸SAT判定
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

	//外積でできる9軸も確認
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

	//衝突情報更新

	KTVECTOR3 centerDelta = other->_center - _center;
	if (Dot(centerDelta, bestAxis) < 0.0f)
		bestAxis = -bestAxis;

	manifold.hasCollision = true;
	manifold.normal = bestAxis.Normalize();
	manifold.penetrationDepth = minOverlap;


	//衝突点の計算
	FixedList<KTVECTOR3, 16> contactPolygon = ComputeContactPolygon(this, other, manifold.normal);
	if (!contactPolygon.empty()) {
		for (auto& p : contactPolygon) {
			ContactPoint cp;
			cp.position = p;
			cp.penetration = minOverlap;	//とりあえず同じ値、より正確にするなら個別に算出
			manifold.contacts.push_back(cp);
		}
	}
	else {//クリップで消えた場合の処理（最近傍中点）
		KTVECTOR3 pointOnA = _center + manifold.normal * (_extents.x + _extents.y + _extents.z);
		KTVECTOR3 pointOnB = other->_center - manifold.normal * (other->_extents.x + other->_extents.y + other->_extents.z);
		ContactPoint cp;
		cp.position = (pointOnA + pointOnB) * 0.5f;
		cp.penetration = minOverlap;
		manifold.contacts.push_back(cp);
	}
	
	//全ての軸で重なっているので衝突している
	return true;
}

bool ColliderBox::CheckVSSphere(const ColliderSphere* other, CollisionManifold& outCollisionManifold) const {
	return other->CheckVSOBB(this, outCollisionManifold);
}

bool ColliderBox::OverlapOnAxis(const ColliderBox* other, const KTVECTOR3& axis) const{

	if (axis.Magnitude() < 1e-6f) return true;

	KTVECTOR3 L = axis.Normalize();

	//自身の投影
	float centerA = Dot(_center, L);
	float rA = _extents.x * fabs(Dot(_axis[0], L)) +
		_extents.y * fabs(Dot(_axis[1], L)) +
		_extents.z * fabs(Dot(_axis[2], L));

	//相手の投影
	float centerB = Dot(other->_center, L);
	float rB = other->_extents.x * fabs(Dot(other->_axis[0], L)) +
		other->_extents.y * fabs(Dot(other->_axis[1], L)) +
		other->_extents.z * fabs(Dot(other->_axis[2], L));

	//中心差
	float distance = fabs(centerA - centerB);

	//分離軸の有無
	return distance <= (rA + rB);

}

bool ColliderBox::OverlapOnAxis(const ColliderBox* other, const KTVECTOR3& axis, float& outOverlap) const
{
	if (axis.Magnitude() < 1e-6f) {
		outOverlap = FLT_MAX;
		return true;
	}

	KTVECTOR3 L = axis.Normalize();

	//自身の投影
	float centerA = Dot(_center, L);
	float rA = _extents.x * fabs(Dot(_axis[0], L)) +
		_extents.y * fabs(Dot(_axis[1], L)) +
		_extents.z * fabs(Dot(_axis[2], L));

	//相手の投影
	float centerB = Dot(other->_center, L);
	float rB = other->_extents.x * fabs(Dot(other->_axis[0], L)) +
		other->_extents.y * fabs(Dot(other->_axis[1], L)) +
		other->_extents.z * fabs(Dot(other->_axis[2], L));

	//中心差
	float distance = fabs(centerA - centerB);
	outOverlap = (rA + rB) - distance;

	//分離軸の有無
	return distance <= (rA + rB);
}

FixedList<KTVECTOR3, 4> ColliderBox::GetFaceVertices(const ColliderBox* box, int axisIndex, int sign){
	FixedList<KTVECTOR3, 4> verts;

	// extents の各軸成分を取り出し
	float ex = box->_extents.x;
	float ey = box->_extents.y;
	float ez = box->_extents.z;

	float extent = (axisIndex == 0) ? ex : (axisIndex == 1 ? ey : ez);

	// face center
	KTVECTOR3 faceCenter = box->_center + box->_axis[axisIndex] * (extent * (float)sign);

	// face のローカル2軸
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

		// +面
		KTVECTOR3 pPos = box->_center + a * extent;
		planes.push_back({ a, Dot(a, pPos) });

		// -面
		KTVECTOR3 pNeg = box->_center - a * extent;
		planes.push_back({ -a, Dot(-a, pNeg) });
	}

	return planes;
}

FixedList<KTVECTOR3, 16> ColliderBox::ClipPolygonAgainstPlane(const FixedList<KTVECTOR3, 16>& polygon, const Plane& plane, float eps){
	FixedList<KTVECTOR3, 16> out;
	if (polygon.empty()) return out;

	auto inside = [&](const KTVECTOR3& v) {
		// 内側条件: Dot(n, v) <= d (+ eps マージン)
		return Dot(plane.n, v) <= plane.d + eps;
		};

	size_t N = polygon.size();
	for (size_t i = 0; i < N; ++i) {
		const KTVECTOR3& A = polygon[i];
		const KTVECTOR3& B = polygon[(i + 1) % N];

		bool inA = inside(A);
		bool inB = inside(B);

		if (inA && inB) {
			// 両方内側 -> B を追加
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
			// A outside, B inside -> 交点 + B
			float da = Dot(plane.n, A) - plane.d;
			float db = Dot(plane.n, B) - plane.d;
			float t = da / (da - db);
			KTVECTOR3 P = A + (B - A) * t;
			out.push_back(P);
			out.push_back(B);
		}
		else {
			// 両方外側 -> 何もしない
		}
	}

	return out;
}


FixedList<KTVECTOR3, 16> ColliderBox::ComputeContactPolygon(const ColliderBox* refBox, const ColliderBox* incBox, const KTVECTOR3& collisionNormal){
	//参照ボックス（refBox）および参照面の決定
	//参照軸は bestAxis に最も近い軸 (abs dot 最大) を選ぶ
	int refAxis = 0;
	float bestDot = fabs(Dot(refBox->_axis[0], collisionNormal));
	for (int i = 1; i < 3; ++i) {
		float d = fabs(Dot(refBox->_axis[i], collisionNormal));
		if (d > bestDot) { bestDot = d; refAxis = i; }
	}

	//法線と軸が同方向→正、逆方向→負
	float s = Dot(collisionNormal, refBox->_axis[refAxis]);
	int refSign = (s >= 0.0f) ? +1 : -1;

	//初期ポリゴン = 参照面の4頂点
	FixedList<KTVECTOR3, 16> poly;
	auto initalVerts =  GetFaceVertices(refBox, refAxis, refSign);

	//型を合わせるためにコピー
	for(const auto& v : initalVerts){
		poly.push_back(v);
	}

	//インシデントボックス(incBox)の6平面を取得
	FixedList<Plane, 8> incPlanes = GetOBBPlanes(incBox);

	//polyを各平面で順にクリップ
	for (const Plane& pl : incPlanes) {
		poly = ClipPolygonAgainstPlane(poly, pl);
		if (poly.empty()) break;
	}

	return poly; // 空ならクリップで消えたことを示す
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
	//GameObjectの情報で更新する
	// _owner->_transform._scale の一番大きい値を反映（歪みを防ぐための均等スケール化）
	float tempScaleRad = _owner->_transform._scale.x;
	float tempScaleHeight = _owner->_transform._scale.y;
	if (tempScaleRad < _owner->_transform._scale.z) tempScaleRad = _owner->_transform._scale.z;

	// 見た目（Capsuleコンポーネント）からパラメータを反映
	Capsule* cap = _owner->GetComponent<Capsule>();
	if (cap) {
		_radius = cap->_radius * tempScaleRad;
		_height = cap->_height * tempScaleHeight;
	}
	else {
		_radius = tempScaleRad * 0.5f;
		_height = tempScaleHeight * 2.0f;
	}

	//AABBの更新
	float halfCylHeight = (std::max)(0.0f, _height - 2.0f * _radius) * 0.5f;

	//カプセルの現在のY軸（Upベクトル）を取得
	KTVECTOR3 up = _owner->GetUp();

	//線分が各軸（X, Y, Z）に対してどれだけ伸びているかを計算し、半径を足す
	KTVECTOR3 extents;
	extents.x = std::abs(up.x) * halfCylHeight + _radius;
	extents.y = std::abs(up.y) * halfCylHeight + _radius;
	extents.z = std::abs(up.z) * halfCylHeight + _radius;

	//AABBの最小値と最大値を更新
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

	//最近傍点2点を求める
	KTVECTOR3 closestPointA, closestPointB;
	ClosestPointSegSeg(s1, e1, s2, e2, closestPointA, closestPointB);

	KTVECTOR3 diff = closestPointB - closestPointA;
	float distSq = diff.MagnitudeSqr();
	float radiusSum = this->_radius + other->_radius;

	if (distSq < radiusSum * radiusSum) {//衝突
		outCollisionManifold.hasCollision = true;
		outCollisionManifold.a = const_cast<ColliderCapsule*>(other);
		outCollisionManifold.b = const_cast<ColliderCapsule*>(this);

		float dist = diff.Magnitude();
		outCollisionManifold.penetrationDepth = radiusSum - dist;
		if (dist > 1e-5f)
		{
			outCollisionManifold.normal = diff.Normalize();
		}
		else {//完全に重なっている場合
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
	//カプセルの内部線分の始点(A)と終点(B)を求める
	float cylinderHeight = (std::max)(0.0f, this->_height - 2.0f * this->_radius);
	GameObject* capsuleObj = this->GetOwner();
	KTVECTOR3 up = capsuleObj->GetUp();

	KTVECTOR3 A = capsuleObj->_transform._position - up * (cylinderHeight * 0.5f);
	KTVECTOR3 B = capsuleObj->_transform._position + up * (cylinderHeight * 0.5f);

	//球の中心点(C)
	KTVECTOR3 C = other->GetOwner()->_transform._position;

	//点Cから線分AB上の最近接点(P)を計算する
	KTVECTOR3 AB = B - A;
	KTVECTOR3 AC = C - A;

	float t = 0.0f;
	float abLengthSqr = AB.MagnitudeSqr();

	//線分が点に退化していないかチェック（ゼロ除算防止）
	if (abLengthSqr > 1e-6f) {
		t = Dot(AC, AB) / abLengthSqr;
	}

	//tを0.0～1.0にクランプして線分上に制限
	t = Clamp(t, 0.0f, 1.0f);

	//最近接点P
	KTVECTOR3 P = A + AB * t;

	//最近接点Pと球の中心Cの距離で衝突判定
	KTVECTOR3 diff = C - P; // PからCへのベクトル
	float distSq = diff.MagnitudeSqr();
	float radiusSum = this->_radius + other->_radius;

	if (distSq <= radiusSum * radiusSum) {//衝突している
		outCollisionManifold.hasCollision = true;

		outCollisionManifold.a = const_cast<ColliderSphere*>(other);
		outCollisionManifold.b = const_cast<ColliderCapsule*>(this);

		float dist = diff.Magnitude();
		outCollisionManifold.penetrationDepth = radiusSum - dist;

		//法線の計算 (カプセルから球へ押し出す方向)
		if (dist > 1e-5f) {
			outCollisionManifold.normal = diff.Normalize(); // 正規化
		}
		else {
			// 完全に中心が重なっている場合は適当な方向に押し出す
			outCollisionManifold.normal = KTVECTOR3(0.0f, 1.0f, 0.0f);
		}

		// 接触点の計算 (球の表面上、または中点)
		ContactPoint cp;
		cp.position = C - outCollisionManifold.normal * other->_radius;
		cp.penetration = outCollisionManifold.penetrationDepth;
		outCollisionManifold.contacts.push_back(cp);

		return true;
	}

	return false;
}

bool ColliderCapsule::CheckVSOBB(const ColliderBox* other, CollisionManifold& outCollisionManifold)const {
	return false;
}

KTMATRIX3 ColliderCapsule::ComputeLocalInertiaTensor(float mass){
	//円柱部分の高さ
	float cylinderHeight = (std::max)(0.0f, _height - 2.0f * _radius);

	//半径が小さすぎる、または質量が0以下の場合はゼロ行列を返す
	if (_radius <= 0.001f || mass <= 0.0f) {
		return KTMATRIX3::Zero();
	}

	const float PI = 3.14159265359f;

	//体積の計算
	float volumeCylinder = PI * _radius * _radius * cylinderHeight;
	float volumeSphere = (4.0f / 3.0f) * PI * _radius * _radius * _radius;
	float volumeTotal = volumeCylinder + volumeSphere;

	if (volumeTotal == 0.0f) return KTMATRIX3::Zero();

	//質量の分配
	float massCylinder = mass * (volumeCylinder / volumeTotal);
	float massSphere = mass * (volumeSphere / volumeTotal);

	//Y軸（長軸）まわりの慣性モーメント
	float iy = (0.5f * massCylinder * _radius * _radius) +
		(0.4f * massSphere * _radius * _radius);

	//X軸・Z軸（短軸）まわりの慣性モーメント
	//円柱部分の X/Z 慣性
	float ixzCylinder = (1.0f / 12.0f) * massCylinder * (3.0f * _radius * _radius + cylinderHeight * cylinderHeight);

	//半球部分の X/Z 慣性（平行軸の定理を適用済み）
	float ixzSphere = massSphere * (0.4f * _radius * _radius +
		0.5f * cylinderHeight * cylinderHeight +
		0.375f * cylinderHeight * _radius);

	float ix = ixzCylinder + ixzSphere;
	float iz = ix;

	//テンソル行列にして返す
	return KTMATRIX3(
		ix, 0.0f, 0.0f,
		0.0f, iy, 0.0f,
		0.0f, 0.0f, iz
	);
}

void ColliderCapsule::ShowUI(){
	ImGui::Checkbox("_wasOverlap", &_wasOverlap);
}
