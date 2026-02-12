#pragma once

#include "Component.h"
#include <cereal/types/base_class.hpp>
#include <cereal/types/polymorphic.hpp>

class BlockSpawner : public Component {
	friend class cereal::access;

public:
	int _cntSpawn = 0;
	void Awake() override;
	void Start() override;
	void Update() override;
	std::string GetComponentName() override { return "BlockSpawner"; }

	template <class Archive>
	void serialize(Archive& ar) {
		ar(cereal::base_class<Component>(this));
		ar(cereal::make_nvp("CntSpawn", _cntSpawn));
	}
};