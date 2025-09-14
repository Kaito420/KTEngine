#pragma once

#include "Component.h"

class ResultManager : public Component {
public:
	bool _isEnter = false;
	void Update() override;
	std::string GetComponentName() override { return "ResultManager"; }
};