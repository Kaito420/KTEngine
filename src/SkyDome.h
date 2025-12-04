#pragma once

#include "Component.h"
#include "Sphere.h"

class SkyDome : public Component{//コンポーネントではなくGameObjectの継承に変更するべき？
public:
	Sphere* _sphere;
	void Awake() override;
	void Update() override;
	void Render()const override;

	std::string GetComponentName() override { return "SkyDome"; }
};
