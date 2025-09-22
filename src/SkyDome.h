#pragma once

#include "Component.h"

class SkyDome : public Component{
public:
	void Update() override;

	std::string GetComponentName() override { return "SkyDome"; }
};
