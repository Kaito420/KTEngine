//=====================================================================================
// Collider.h
// Author:Kaito Aoki
// Date:2025/09/08
//=====================================================================================

#ifndef _COLLIDER_H_
#define _COLLIDER_H_

#include "Component.h"
#include "ktvector.hpp"


class ColliderBox;

class Collider : public Component
{
protected:
	KTVECTOR3 _center;
	bool _isOverlap;
	bool _wasOverlap;

	virtual bool BeginOverlap() { return false; }
	virtual bool StayOverlap() { return false; }
	virtual bool EndOverlap() { return false; }

public:
	virtual bool IsOverlap(ColliderBox* other) { return false; }
	std::string GetComponentName() { return "Collider"; }

};

class ColliderBox : public Collider
{
public:
	KTVECTOR3 _axis[3];
	KTVECTOR3 _extents;

	void Awake() override;

	// GameObjectÇÃèÓïÒÇ≈çXêVÇ∑ÇÈ
	void Update() override;

	void Render()const override;

	bool IsOverlap(ColliderBox* other) override;

	bool OverlapOnAxis(const ColliderBox* other, const KTVECTOR3& axis)const;

	std::string GetComponentName() { return "ColliderBox"; }

	void ShowUI()override;

	void OnCollisionEnter(Collider* other)override { _isOverlap = true; }


};

#endif // !_COLLIDER_H_