//=====================================================================================
// KillY.h
// Author:Kaito Aoki
// Date:2025/12/18
//=====================================================================================

#ifndef _KILLY_H_
#define _KILLY_H_

#include "Component.h"
#include "ktvector.hpp"
#include <cereal/types/base_class.hpp>
#include <cereal/types/polymorphic.hpp>

class KillY : public Component{
	friend class cereal::access;
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

	template <class Archive>
	void serialize(Archive& ar) {
		ar(cereal::base_class<Component>(this));
		ar(cereal::make_nvp("Threshold", _threshold));
		ar(cereal::make_nvp("ResetPosition", _resetPosition));
	}
};

#endif // !_KILLY_H_