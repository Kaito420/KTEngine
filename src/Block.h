#pragma once

#include "GameObject.h"

class Piece;

class Block : public GameObject {

public:
	Piece* _piece = nullptr;
	void Awake() override;

	void Update() override;
};