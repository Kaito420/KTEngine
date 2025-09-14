#pragma once

#include "Component.h"

class TitleManager : public Component {
public:
	bool _isEnter = false;
	void Update() override;
	std::string GetComponentName() override { return "TitleManager"; }
};