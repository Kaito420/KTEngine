//=====================================================================================
// Collider.cpp
// Author:Kaito Aoki
// Date:2025/09/08
//=====================================================================================

#include "Collider.h"
#include "GameObject.h"
#include "Manager.h"
#include "Scene.h"
#include "RendererDX11.h"

void CollisionManifold::CreateSphereMesh(float radius, int sliceCount, int stackCount, std::vector<Vertex>& vertices, std::vector<UINT>& indices) {
	vertices.clear();
	indices.clear();

	//トップ
	vertices.push_back({ XMFLOAT3(0.0f, radius, 0.0f),
						 XMFLOAT3(0.0f, 1.0f, 0.0f),
						 XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f),
						 XMFLOAT2(0.0f, 0.0f)
		});

	float phiStep = XM_PI / stackCount;
	float thetaStep = 2.0f * XM_PI / sliceCount;

	for (UINT i = 1; i <= stackCount - 1; ++i)
	{
		float phi = i * phiStep;

		for (UINT j = 0; j <= sliceCount; ++j)
		{
			float theta = j * thetaStep;

			XMFLOAT3 pos(
				radius * sinf(phi) * cosf(theta),
				radius * cosf(phi),
				radius * sinf(phi) * sinf(theta));

			XMFLOAT3 normal;
			XMStoreFloat3(&normal, XMVector3Normalize(XMLoadFloat3(&pos)));

			XMFLOAT4 diffuse = { 1.0f, 1.0f, 1.0f, 1.0f };

			XMFLOAT2 texCoord(theta / XM_2PI, phi / XM_PI);

			vertices.push_back({ pos, normal, diffuse, texCoord });
		}
	}

	// ボトム頂点
	vertices.push_back({ XMFLOAT3(0.0f, -radius, 0.0f),
						 XMFLOAT3(0.0f, -1.0f, 0.0f),
						 XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f),
						 XMFLOAT2(0.0f, 1.0f) });

	// top stack (top pole to first ring)
	for (UINT i = 0; i < sliceCount; ++i)
	{
		indices.push_back(0); // top pole index
		indices.push_back((i + 1) % sliceCount + 1);
		indices.push_back(i + 1);
	}

	// middle stacks
	UINT baseIndex = 1;
	UINT ringVertexCount = sliceCount + 1;

	for (UINT i = 0; i < stackCount - 2; ++i)
	{
		for (UINT j = 0; j < sliceCount; ++j)
		{
			indices.push_back(baseIndex + i * ringVertexCount + j);
			indices.push_back(baseIndex + i * ringVertexCount + j + 1);
			indices.push_back(baseIndex + (i + 1) * ringVertexCount + j);

			indices.push_back(baseIndex + (i + 1) * ringVertexCount + j);
			indices.push_back(baseIndex + i * ringVertexCount + j + 1);
			indices.push_back(baseIndex + (i + 1) * ringVertexCount + j + 1);
		}
	}

	// bottom stack
	UINT southPoleIndex = (UINT)vertices.size() - 1;
	baseIndex = southPoleIndex - ringVertexCount;

	for (UINT i = 0; i < sliceCount; ++i)
	{
		indices.push_back(southPoleIndex);
		indices.push_back(baseIndex + i);
		indices.push_back(baseIndex + i + 1);
	}

}

CollisionManifold::CollisionManifold()
{
	std::vector<Vertex> vertices;
	std::vector<UINT> indices;
	CreateSphereMesh(0.3f, 10, 10, vertices, indices);
	_indexCount = indices.size();

	//頂点バッファ生成
	D3D11_BUFFER_DESC bd = {};
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(Vertex) * vertices.size();
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = 0;

	D3D11_SUBRESOURCE_DATA sd = {};
	sd.pSysMem = vertices.data();

	RendererDX11::GetDevice()->CreateBuffer(&bd, &sd, &_vertexBuffer);

	//インデックスバッファ
	bd.ByteWidth = sizeof(UINT) * indices.size();
	bd.BindFlags = D3D11_BIND_INDEX_BUFFER;

	sd.pSysMem = indices.data();

	RendererDX11::GetDevice()->CreateBuffer(&bd, &sd, &_indexBuffer);

}

void CollisionManifold::Render()const {
	//for (auto& cp : contacts) 
	if(contacts.size() != 0){
		UINT stride = sizeof(Vertex);
		UINT offset = 0;

		RendererDX11::GetContext()->IASetVertexBuffers(0, 1, _vertexBuffer.GetAddressOf(), &stride, &offset);
		RendererDX11::GetContext()->IASetIndexBuffer(_indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);

		//マトリクス設定
		//平行移動行列
		XMMATRIX translation = XMMatrixTranslation(contacts[0].position.x, contacts[0].position.y, contacts[0].position.z);

		XMMATRIX rotation = XMMatrixIdentity();

		//スケーリング行列
		XMMATRIX scaling = XMMatrixIdentity();

		//ワールド行列
		XMMATRIX worldMatrix = scaling * rotation * translation;

		RendererDX11::SetWorldMatrix(worldMatrix);

		// プリミティブトポロジ設定
		RendererDX11::GetContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		MATERIAL material = {};
		material.Diffuse = { 1.0f, 1.0f, 1.0f, 1.0f };
		material.TextureEnable = false;
		RendererDX11::SetMaterial(material);

		// シェーダーリソースビュー設定
		// ポリゴン描画
		RendererDX11::GetContext()->DrawIndexed(_indexCount, 0, 0);

	}
}

void ColliderBox::Awake() {
	Manager::GetCurrentScene()->GetPhysicsSystem()->RegisterCollider(this);
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

void ColliderBox::OnDestroy()
{
	Manager::GetCurrentScene()->GetPhysicsSystem()->RemoveCollider(this);
}

void ColliderBox::Update() {
	_center = _owner->_transform._position;
	_axis[0] = _owner->GetRight();
	_axis[1] = _owner->GetUp();
	_axis[2] = _owner->GetForward();

	_extents = _owner->_transform._scale * 0.5f;
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

CollisionManifold ColliderBox::CheckVSOBB(ColliderBox* other) {
	CollisionManifold manifold;
	manifold.a = other;
	manifold.b = this;

	float minOverlap = FLT_MAX;	//最小の重なり量
	KTVECTOR3 bestAxis;			//最小の重なり軸

	//各軸について投影してチェック
	//分離軸SAT判定
	for(int i = 0; i<3; i++) {
		float overlap = 0.0f;
		if (!OverlapOnAxis(other, _axis[i], overlap)) return manifold;
		if(overlap < minOverlap) {
			minOverlap = overlap;
			bestAxis = _axis[i];
		}

		if (!OverlapOnAxis(other, other->_axis[i], overlap)) return manifold;
		if (overlap < minOverlap) {
			minOverlap = overlap;
			bestAxis = other->_axis[i];
		}
	}

	//外積でできる9軸も確認
	for (int i = 0; i < 3; i++) {
		for (int j = 0; j < 3; j++) {
			KTVECTOR3 axis = Cross(_axis[i], other->_axis[j]);
			if (axis.Magnitude() < DBL_EPSILON) continue;

			float overlap = 0.0f;
			if (!OverlapOnAxis(other, axis.Normalize(), overlap)) return manifold;
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
	std::vector<KTVECTOR3> contactPolygon = ComputeContactPolygon(this, other, manifold.normal);
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
	return manifold;
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

std::vector<KTVECTOR3> ColliderBox::GetFaceVertices(const ColliderBox* box, int axisIndex, int sign){
	std::vector<KTVECTOR3> verts;
	verts.reserve(4);

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

std::vector<Plane> ColliderBox::GetOBBPlanes(const ColliderBox* box){
	std::vector<Plane> planes;
	planes.reserve(6);

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

std::vector<KTVECTOR3> ColliderBox::ClipPolygonAgainstPlane(const std::vector<KTVECTOR3>& polygon, const Plane& plane, float eps){
	std::vector<KTVECTOR3> out;
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

KTVECTOR3 ColliderBox::ComputePolygonCentroid(const std::vector<KTVECTOR3>& polygon, const KTVECTOR3& refNormal){
	KTVECTOR3 centroid(0.0f, 0.0f, 0.0f);
	if (polygon.size() == 0) return centroid;
	if (polygon.size() == 1) return polygon[0];
	if (polygon.size() == 2) return (polygon[0] + polygon[1]) * 0.5f;

	// 三角分割: v0 を基準に (v0, vi, vi+1)
	KTVECTOR3 v0 = polygon[0];
	double areaSum = 0.0;
	KTVECTOR3 cSum(0.0f, 0.0f, 0.0f);

	for (size_t i = 1; i + 1 < polygon.size(); ++i) {
		KTVECTOR3 a = polygon[i] - v0;
		KTVECTOR3 b = polygon[i + 1] - v0;
		KTVECTOR3 cross = Cross(a, b);
		double triArea = 0.5 * (double)cross.Magnitude(); // 面積
		if (triArea <= 1e-12) continue;
		// 三角形の重心
		KTVECTOR3 triCentroid = (v0 + polygon[i] + polygon[i + 1]) * (1.0f / 3.0f);
		cSum += triCentroid * (float)triArea;
		areaSum += triArea;
	}

	if (areaSum <= 1e-12) {
		// 面積がほぼ0 -> 頂点平均で代替
		KTVECTOR3 s(0, 0, 0);
		for (auto& p : polygon) s += p;
		return s * (1.0f / (float)polygon.size());
	}

	centroid = cSum * (1.0f / (float)areaSum);
	return centroid;
}

std::vector<KTVECTOR3> ColliderBox::ComputeContactPolygon(const ColliderBox* refBox, const ColliderBox* incBox, const KTVECTOR3& collisionNormal){
	// 1) 参照ボックス（refBox）および参照面の決定
 //    参照軸は bestAxis に最も近い軸 (abs dot 最大) を選ぶ
	int refAxis = 0;
	float bestDot = fabs(Dot(refBox->_axis[0], collisionNormal));
	for (int i = 1; i < 3; ++i) {
		float d = fabs(Dot(refBox->_axis[i], collisionNormal));
		if (d > bestDot) { bestDot = d; refAxis = i; }
	}

	// sign: +1 if collisionNormal is in same direction as axis, else -1
	float s = Dot(collisionNormal, refBox->_axis[refAxis]);
	int refSign = (s >= 0.0f) ? +1 : -1;

	// 初期ポリゴン = 参照面の4頂点
	std::vector<KTVECTOR3> poly = GetFaceVertices(refBox, refAxis, refSign);

	// 2) インシデントボックス (incBox) の 6平面を取得
	std::vector<Plane> incPlanes = GetOBBPlanes(incBox);

	// 3) poly を各平面で順にクリップ
	for (const Plane& pl : incPlanes) {
		poly = ClipPolygonAgainstPlane(poly, pl);
		if (poly.empty()) break;
	}

	return poly; // 空ならクリップで消えたことを示す
}

void ColliderBox::ShowUI() {
	ImGui::Checkbox("_wasOverlap", &_wasOverlap);
}