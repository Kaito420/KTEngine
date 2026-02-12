#pragma once

#include "Component.h"
#include "Sphere.h"
#include <string>
#include <cereal/types/base_class.hpp>
#include <cereal/types/polymorphic.hpp>

class SkyDome : public Component{//コンポーネントではなくGameObjectの継承に変更するべき？
	friend class cereal::access;
public:
	Sphere* _sphere;
	void Awake() override;
	void Update() override;
	void Render()const override;

	std::string GetComponentName() override { return "SkyDome"; }

	template <class Archive>
	void serialize(Archive& ar) {
		ar(cereal::base_class<Component>(this));
	}
};
