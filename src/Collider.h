//=====================================================================================
// Collider.h
// Author:Kaito Aoki
// Date:2025/09/08
//=====================================================================================

#ifndef _COLLIDER_H_
#define _COLLIDER_H_

#include "Component.h"
#include "ktvector.hpp"
#include <unordered_set>
#include <vector>
#include "RendererDX11.h"

class ColliderBox;
class ColliderSphere;

class Plane {
public:
	KTVECTOR3 n;	//法線ベクトル
	float d;		//平面オフセット
};

struct ContactPoint {
	KTVECTOR3 position; // 接触点の位置
	float penetration; // 浸入深度
};

struct CollisionManifold {
	Collider* a;	// コリジョンA
	Collider* b;	// コリジョンB
	KTVECTOR3 normal; // 衝突法線(B->A方向)
	float penetrationDepth = 0.0f;
	std::vector<ContactPoint> contacts; // 接触点のリスト
	bool hasCollision = false; // 衝突が発生しているかどうか
	
	//======================
	// debug用接触点リストの描画
	//======================
	ComPtr<ID3D11Buffer> _vertexBuffer;
	ComPtr<ID3D11Buffer> _indexBuffer;
	ID3D11ShaderResourceView* _texture = nullptr;
	int _indexCount = 0;
	CollisionManifold();
	void CreateSphereMesh(float radius, int sliceCount, int stackCount, std::vector<Vertex>& vertices, std::vector<UINT>& indices);
	void Render()const;

};

class Collider : public Component
{

protected:
	KTVECTOR3 _center;

	ComPtr<ID3D11Buffer> _vertexBuffer;
	ComPtr<ID3D11Buffer> _indexBuffer;

public:
	bool _isOverlap;
	bool _wasOverlap;


	/// <summary>
	/// 現在フレームで当たっているコリジョン
	/// </summary>
	std::unordered_set<Collider*> _currentOverlaps;
	/// <summary>
	/// 前フレームで当たっていたコリジョン
	/// </summary>
	std::unordered_set<Collider*> _previousOverlaps;

	void BeginFrame() {
		_currentOverlaps.clear();
	}

	void EndFrame() {
		_wasOverlap = _isOverlap;
		_isOverlap = false;

		_previousOverlaps = _currentOverlaps;
	}

	virtual bool Collide(Collider* other, CollisionManifold& outCollisionManifold) = 0;
	virtual bool CollideWith(ColliderBox* other, CollisionManifold& outCollisionManifold) = 0;
	virtual bool CollideWith(ColliderSphere* other, CollisionManifold& outCollisionManifold) = 0;

	std::string GetComponentName() { return "Collider"; }

};

class ColliderSphere : public Collider {
public:
	float _radius;

	void Awake() override;

	void OnDestroy() override;

	// GameObjectの情報で更新する
	void Update() override;

	//デバッグ描画用
	void Render()const override;

	bool Collide(Collider* other, CollisionManifold& outCollisionManifold) {
		return other->CollideWith(this, outCollisionManifold);	//ここで自身と相手が入れ替わる
	}

	bool CollideWith(ColliderBox* other, CollisionManifold& outCollisionManifold) {
		return false;
	}

	bool CollideWith(ColliderSphere* other, CollisionManifold& outCollisionManifold) {
		return false;
	}

	bool CheckVSSphere(const ColliderSphere* other, CollisionManifold& outCollisionManifold)const;

	std::string GetComponentName() { return "ColliderSphere"; }

};

class ColliderBox : public Collider
{
public:
	KTVECTOR3 _axis[3];
	KTVECTOR3 _extents;

	void Awake() override;

	void OnDestroy() override;

	// GameObjectの情報で更新する
	void Update() override;

	//デバッグ描画用
	void Render()const override;


	bool Collide(Collider* other, CollisionManifold& outCollisionManifold) {
		return other->CollideWith(this, outCollisionManifold);	//ここで自身と相手が入れ替わる
	}

	bool CollideWith(ColliderBox* other, CollisionManifold& outCollisionManifold) {
		return CheckVSOBB(other, outCollisionManifold);
	}

	bool CollideWith(ColliderSphere* other, CollisionManifold& outCollisionManifold) {
		return false;
	}

	bool CheckVSOBB(const ColliderBox* other, CollisionManifold& outCollisionManifold)const;
	bool CheckVSSphere(const ColliderSphere* other, CollisionManifold& outCollisionManifold)const;

	bool OverlapOnAxis(const ColliderBox* other, const KTVECTOR3& axis)const;

	bool OverlapOnAxis(const ColliderBox* other, const KTVECTOR3& axis, float& outOverlap)const;

	static std::vector<KTVECTOR3> GetFaceVertices(const ColliderBox* box, int axisIndex, int sign);

	static std::vector<Plane> GetOBBPlanes(const ColliderBox* box);

	static std::vector<KTVECTOR3> ClipPolygonAgainstPlane(const std::vector<KTVECTOR3>& polygon, const Plane& plane, float eps = 1e-6f);

	static KTVECTOR3 ComputePolygonCentroid(const std::vector<KTVECTOR3>& polygon, const KTVECTOR3& refNormal);

	static std::vector<KTVECTOR3> ComputeContactPolygon(const ColliderBox* refBox, const ColliderBox* incBox, const KTVECTOR3& collisionNormal);


	std::string GetComponentName() { return "ColliderBox"; }

	void ShowUI()override;


};

#endif // !_COLLIDER_H_