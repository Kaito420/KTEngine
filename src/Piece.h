#pragma once

#include "Component.h"
#include <cereal/types/base_class.hpp>
#include <cereal/types/polymorphic.hpp>

class Piece : public Component {
	friend class cereal::access;
public:
	enum Type {
		Marcury = 1,
		Mars,
		Venus,
		Earth,
		Saturn,
		Jupiter,
		Sun,
		Max
	};
	Type _type;
	void Awake() override;
	void OnCollisionEnter(Collider* other) override;
	void ShowUI()override;

	std::string GetComponentName() override { return "Piece"; }

	template <class Archive>
	void serialize(Archive& ar) {
		ar(cereal::base_class<Component>(this));
		ar(cereal::make_nvp("Type", _type));
	}
};