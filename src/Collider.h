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

class ColliderBox;

class Collider : public Component
{
protected:
	KTVECTOR3 _center;


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

	std::string GetComponentName() { return "ColliderBox"; }

	void ShowUI()override;


};

#endif // !_COLLIDER_H_