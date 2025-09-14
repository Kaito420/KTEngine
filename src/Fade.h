#pragma once

#include "Square.h"

class Fade : public Square {
public:
	float _alpha = 0.0f;
	float _fadeSpeed = 0.01f;
	float _color = 0.0f;
	void Update() override;
	void Render()const override;

	void OnDestroy()override;
};