#pragma once

#include "GameObject.h"
#include <cereal/types/base_class.hpp>
#include <cereal/types/polymorphic.hpp>

class Piece;

class Block : public GameObject {
	friend class cereal::access;
public:
	Piece* _piece = nullptr;
	void Awake() override;

	void Update() override;

	template <class Archive>
	void serialize(Archive& ar) {
		ar(cereal::base_class<GameObject>(this));
	}
};