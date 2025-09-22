#pragma once

#include "Component.h"
#include "RendererDX11.h"
#include "ktvector.hpp"

class Particle : public Component {
	ID3D11Buffer* _VertexBuffer;
	ID3D11ShaderResourceView* _texture;
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
};