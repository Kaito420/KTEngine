#pragma once

#include "GameObject.h"
#include <cereal/types/base_class.hpp>
#include <cereal/types/polymorphic.hpp>

class FadeObject : public GameObject {
	friend class cereal::access;
public:
	void Awake() override;

	template <class Archive>
	void serialize(Archive& ar) {
		ar(cereal::base_class<GameObject>(this));
	}
};