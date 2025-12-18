//=====================================================================================
// KillY.h
// Author:Kaito Aoki
// Date:2025/12/18
//=====================================================================================

#ifndef _KILLY_H_
#define _KILLY_H_

#include "Component.h"
#include "ktvector.hpp"

class KillY : public Component
{
private:
	float _threshold = -30.0f; // Y座標の閾値
	KTVECTOR3 _resetPosition = { 0.0f, 0.0f, 0.0f }; // リセット後のY座標
	class RigidBody* _rigidBody = nullptr;
public:
	void SetThreshold(float threshold) { _threshold = threshold; }
	void SetResetPosition(KTVECTOR3 resetPosition) { _resetPosition = resetPosition; }
	void Awake() override;
	void Update() override;

	void ShowUI() override;
	std::string GetComponentName()override { return "KillY"; }
};

#endif // !_KILLY_H_