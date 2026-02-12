#pragma once

#include "Square.h"
#include <cereal/types/base_class.hpp>
#include <cereal/types/polymorphic.hpp>

class Fade : public Square {
	friend class cereal::access;
public:
	float _alpha = 0.0f;
	float _fadeSpeed = 0.01f;
	float _color = 0.0f;
	void Update() override;
	void Render()const override;

	void OnDestroy()override;

	std::string GetComponentName() override { return "Fade"; }

	template <class Archive>
	void serialize(Archive& ar) {
		ar(cereal::base_class<Square>(this));
		ar(cereal::make_nvp("Alpha", _alpha));
		ar(cereal::make_nvp("FadeSpeed", _fadeSpeed));
		ar(cereal::make_nvp("Color", _color));
	}
};