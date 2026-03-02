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
#include <array>
#include <cereal/types/base_class.hpp>
#include <cereal/types/polymorphic.hpp>

class ColliderBox;
class ColliderSphere;
class ColliderCapsule;

template <typename T, int Capacity>
class FixedList {
public:
	std::array<T, Capacity> _data;
	int _size = 0;

	void clear() {
		_size = 0;
	}

	void push_back(const T& value) {
		if (_size < Capacity) {
			_data[_size] = value;
			_size++;
		}
		else
			assert(false && "FixedList capacity exceeded");
	}

	size_t size()const { return _size; }
	bool empty()const { return _size == 0; }

	T& operator[](int index) { return _data[index]; }
	const T& operator[](int indedx)const { return _data[indedx]; }

	T* begin() { return _data.data(); }
	T* end() { return _data.data() + _size; }
	const T* begin()const { return _data.data(); }
	const T* end()const { return _data.data() + _size; }

};

class Plane {
public:
	KTVECTOR3 n;	//法線ベクトル
	float d;		//平面オフセット
};

struct ContactPoint {
	KTVECTOR3 position; // 接触点の位置
	float penetration; // 浸入深度
	float normalImpulseSum = 0.0f; // 法線方向の累積インパルス
	KTVECTOR3 tangentImpulseSum = { 0.0f, 0.0f, 0.0f }; // 接線方向の累積インパルス
	float velocityBias = 0.0f; // 反発や位置補正のための目標速度
};

struct AABB {
	KTVECTOR3 min;
	KTVECTOR3 max;
	bool CheckOverlap(const AABB& other)const {
		if (max.x < other.min.x || min.x > other.max.x) return false;
		if (max.y < other.min.y || min.y > other.max.y) return false;
		if (max.z < other.min.z || min.z > other.max.z) return false;
		return true;
	}
};

struct CollisionManifold {
	Collider* a;	// コリジョンA
	Collider* b;	// コリジョンB
	KTVECTOR3 normal; // 衝突法線(B->A方向)
	float penetrationDepth = 0.0f;
	std::vector<ContactPoint> contacts; // 接触点のリスト
	bool hasCollision = false; // 衝突が発生しているかどうか

};

class Collider : public Component{
	friend class cereal::access;
protected:
	KTVECTOR3 _center;

	ComPtr<ID3D11Buffer> _vertexBuffer;
	ComPtr<ID3D11Buffer> _indexBuffer;

public:
	AABB _aabb;
	bool _hasChangedScale = false;

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

	virtual KTMATRIX3 ComputeLocalInertiaTensor(float mass) = 0;

	std::string GetComponentName() { return "Collider"; }

	template <class Archive>
	void serialize(Archive& ar) {
		ar(cereal::base_class<Component>(this));
		ar(cereal::make_nvp("Center", _center));
	}

};

class ColliderSphere : public Collider {
	friend class cereal::access;
private:
	float _oldRadius;
public:
	float _radius = 0.5;

	void Awake() override;
	void Start() override;
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
		return CheckVSSphere(other, outCollisionManifold);
	}

	bool CheckVSSphere(const ColliderSphere* other, CollisionManifold& outCollisionManifold)const;
	bool CheckVSOBB(const ColliderBox* other, CollisionManifold& outCollisionManifold)const;

	KTMATRIX3 ComputeLocalInertiaTensor(float mass)override;

	std::string GetComponentName() { return "ColliderSphere"; }

	void ShowUI()override;

	template <class Archive>
	void serialize(Archive& ar) {
		ar(cereal::base_class<Collider>(this));
		ar(cereal::make_nvp("Radius", _radius));
	}
};

class ColliderBox : public Collider{
	friend class cereal::access;
private:
	KTVECTOR3 _oldExtents;
public:
	KTVECTOR3 _axis[3];	//[0] : X, [1] : Y, [2] : Z
	KTVECTOR3 _extents;


	void Awake() override;
	void Start() override;
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
		return CheckVSSphere(other, outCollisionManifold);
	}

	bool CheckVSOBB(const ColliderBox* other, CollisionManifold& outCollisionManifold)const;
	bool CheckVSSphere(const ColliderSphere* other, CollisionManifold& outCollisionManifold)const;

	bool OverlapOnAxis(const ColliderBox* other, const KTVECTOR3& axis)const;

	bool OverlapOnAxis(const ColliderBox* other, const KTVECTOR3& axis, float& outOverlap)const;

	static FixedList<KTVECTOR3, 4> GetFaceVertices(const ColliderBox* box, int axisIndex, int sign);

	static FixedList<Plane, 8> GetOBBPlanes(const ColliderBox* box);

	static FixedList<KTVECTOR3, 16> ClipPolygonAgainstPlane(const FixedList<KTVECTOR3, 16>& polygon, const Plane& plane, float eps = 1e-6f);

	static FixedList<KTVECTOR3, 16> ComputeContactPolygon(const ColliderBox* refBox, const ColliderBox* incBox, const KTVECTOR3& collisionNormal);

	KTMATRIX3 ComputeLocalInertiaTensor(float mass)override;

	std::string GetComponentName() { return "ColliderBox"; }

	void ShowUI()override;

	template <class Archive>
	void serialize(Archive& ar) {
		ar(cereal::base_class<Collider>(this));
		ar(cereal::make_nvp("Extents", _extents));

	}
};

//class CollideCapsule : public Collider {
//
//};

#endif // !_COLLIDER_H_