#pragma once

#include "Component.h"
#include "Renderer.h"
#include "ktvector.hpp"
#include <cereal/types/base_class.hpp>
#include <cereal/types/polymorphic.hpp>

class Particle : public Component {
	friend class cereal::access;
	std::unique_ptr<VERTEX_BUFFER> _vertexBuffer;
	const TEXTURE* _texture = nullptr;
	struct PARTICLE {
		bool enable;
		int Life;
		KTVECTOR3 Position;
		KTVECTOR3 Velocity;
	};
	static const int PARTICLE_MAX = 100;
	PARTICLE _particle[PARTICLE_MAX];

public:
	void Awake()override;
	void Update()override;
	void Render()const override;
	std::string GetComponentName() override { return "Particle"; }
	template <class Archive>
	void serialize(Archive& ar) {
		ar(cereal::base_class<Component>(this));
	}
};