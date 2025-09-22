#pragma once

#include "Component.h"

class BlockSpawner : public Component {

public:
	int _cntSpawn = 0;
	void Awake() override;
	void Start() override;
	void Update() override;
	std::string GetComponentName() override { return "BlockSpawner"; }
};