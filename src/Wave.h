//=====================================================================================
// Wave.h
// Author:Kaito Aoki
// Date:2025/11/14
//=====================================================================================

#ifndef _WAVE_H_
#define _WAVE_H_

#include "Component.h"
#include "RendererDX11.h"
#include <cereal/types/base_class.hpp>
#include <cereal/types/polymorphic.hpp>

class Wave : public Component{
	friend class cereal::access;

private:

	ID3D11Buffer* m_VertexBuffer{};
	ID3D11Buffer* m_IndexBuffer{};

	ID3D11ShaderResourceView* m_Texture{};
	ID3D11ShaderResourceView* m_TextureEnv{};


	ID3D11VertexShader* m_VertexShader{};
	ID3D11PixelShader* m_PixelShader{};
	ID3D11InputLayout* m_VertexLayout{};

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