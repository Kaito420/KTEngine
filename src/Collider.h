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

class ColliderBox;

class Plane {
public:
	KTVECTOR3 n;	//法線ベクトル
	float d;		//平面オフセット
};

class Collider : public Component
{
	struct CollisionInfo {
		Collider* _other;			//衝突相手
		KTVECTOR3 _collisionPoint;	//衝突座標
		KTVECTOR3 _collisionNormal;	//衝突法線
		float _penetrationDepth;	//深度
	};

protected:
	KTVECTOR3 _center;


public:
	bool _isOverlap;
	bool _wasOverlap;

	CollisionInfo _collisionInfo;

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
	virtual bool IsOverlap(Collider* other) = 0;
	virtual bool IsOverlapWith(ColliderBox* other) = 0;
	std::string GetComponentName() { return "Collider"; }

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

	void Render()const override;

	bool IsOverlap(Collider* other) {
		return other->IsOverlapWith(this);
	}

	bool IsOverlapWith(ColliderBox* other) {
		return CheckVSOBB(other);
	}

	bool CheckVSOBB(ColliderBox* other);

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