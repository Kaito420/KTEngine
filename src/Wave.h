//=====================================================================================
// Wave.h
// Author:Kaito Aoki
// Date:2025/11/14
//=====================================================================================

#ifndef _WAVE_H_
#define _WAVE_H_

#include "Component.h"
#include "Renderer.h"
#include <cereal/types/base_class.hpp>
#include <cereal/types/polymorphic.hpp>

class Wave : public Component{
	friend class cereal::access;

private:

	std::unique_ptr<VERTEX_BUFFER> m_vertexBuffer;
	std::unique_ptr<INDEX_BUFFER> m_indexBuffer;

	const TEXTURE* m_texture = nullptr;
	const TEXTURE* m_textureEnv = nullptr;

	Vertex m_Vertex[21][21];

	float m_Time{};

public:
	void Awake() override;
	void OnDestroy() override;
	void Update() override;
	void Render()const override;


	std::string GetComponentName()override { return "Wave"; }

	template <class Archive>
	void serialize(Archive& ar) {
		ar(cereal::base_class<Component>(this));
	}
};

#endif // !_WAVE_H_