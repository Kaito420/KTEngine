#pragma once

#include "Scene.h"
#include <cereal/types/base_class.hpp>
#include <cereal/types/polymorphic.hpp>

class SceneResult : public Scene {
	friend class cereal::access;
public:
	void Initialize() override;

	template <class Archive>
	void serialize(Archive& ar) {
		ar(cereal::base_class<Scene>(this));
	}
};