#pragma once

#include "Component.h"
#include "Square.h"
#include <cereal/types/base_class.hpp>
#include <cereal/types/polymorphic.hpp>
class Billboard : public Square {
	friend class cereal::access;
public:

	void Awake() override;
	void Update() override;
	void Render()const override;
	std::string GetComponentName() override { return "Billboard"; }
	template <class Archive>
	void serialize(Archive& ar) {
		ar(cereal::base_class<Square>(this));
	}
};