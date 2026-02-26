//=====================================================================================
// Capsule.h
// Author:Kaito Aoki
// Date:2026/02/26
//=====================================================================================

#ifndef _CAPSULE_H
#define _CAPSULE_H

#include "Component.h"
#include "RendererDX11.h"
#include <cereal/types/base_class.hpp>
#include <cereal/types/polymorphic.hpp>

class Capsule : public Component {
	friend class cereal::access;
private:
	ComPtr<ID3D11Buffer> _vertexBuffer;
	ComPtr<ID3D11Buffer> _indexBuffer;
	int _indexCount = 0;
	void CreateCapsuleMesh(float height, float radius, int latiudes, int longitudes,
		std::vector<Vertex>& vertices, std::vector<UINT>& indices);

public:
	ComPtr<ID3D11ShaderResourceView> _texture = nullptr;
	int _latiudes = 16;
	int _longitudes = 16;

	float _height = 2.0f;
	float _radius = 0.5f;
	void Awake() override;
	void Render()const override;

	void ShowUI() override;

	std::string GetComponentName()override { return "Capsule"; }

	template <class Archive>
	void serialize(Archive& ar) {
		ar(cereal::base_class<Component>(this));
	}
};

#endif //!_CAPSULE_H