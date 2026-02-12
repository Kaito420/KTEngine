#pragma once

#include "Component.h"
#include "Collider.h"
#include <cereal/types/base_class.hpp>
#include <cereal/types/polymorphic.hpp>

class CubeKiller : public Component {
	friend class cereal::access;
public:
	int _cntKill = 0;
	void Update() override;
	void OnCollisionEnter(Collider* other)override;
	
	std::string GetComponentName() override { return "CubeKiller"; }

	template <class Archive>
	void serialize(Archive& ar) {
		ar(cereal::base_class<Component>(this));
		ar(cereal::make_nvp("CntKill", _cntKill));
	}
};