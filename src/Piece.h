#pragma once

#include "Component.h"

class Piece : public Component {
public:
	enum Type {
		Marcury = 1,
		Mars,
		Venus,
		Earth,
		Saturn,
		Jupiter,
		Sun,
		Max
	};
	Type _type;
	void Awake() override;
	void OnCollisionEnter(Collider* other) override;

	std::string GetComponentName() override { return "Piece"; }
};