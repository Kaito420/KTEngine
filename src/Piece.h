#pragma once

#include "Component.h"

class Piece : public Component {
	void OnCollisionEnter(Collider* other) override;

	std::string GetComponentName() override { return "Piece"; }
};