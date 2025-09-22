#pragma once

#include "Component.h"
#include "Square.h"

class Billboard : public Square {

public:

	void Awake() override;
	void Update() override;
	void Render()const override;
	std::string GetComponentName() override { return "Billboard"; }
};