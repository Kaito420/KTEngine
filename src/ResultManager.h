#pragma once

#include "Component.h"
#include <string>
#include <cereal/types/base_class.hpp>
#include <cereal/types/polymorphic.hpp>

class ResultManager : public Component {
	friend class cereal::access;
public:
	bool _isEnter = false;
	void Update() override;
	std::string GetComponentName() override { return "ResultManager"; }
	template <class Archive>
	void serialize(Archive& ar) {
		ar(cereal::base_class<Component>(this));
		ar(cereal::make_nvp("IsEnter", _isEnter));
	}
};