#pragma once

#include "Component.h"

class BlockSpawner : public Component {

	public:
	void Update() override;
	std::string GetComponentName() override { return "BlockSpawner"; }
};