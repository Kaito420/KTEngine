//=====================================================================================
// Sphere.h
// Author:Kaito Aoki
// Date:2025/09/05
//=====================================================================================

#ifndef _SPHERE_H_
#define _SPHERE_H_

#include "Component.h"
#include "RendererDX11.h"
#include <vector>
#include <string>
#include <cereal/types/base_class.hpp>
#include <cereal/types/polymorphic.hpp>

class Sphere : public Component {
	friend class cereal::access;
private:
	ComPtr<ID3D11Buffer> _vertexBuffer;
	ComPtr<ID3D11Buffer> _indexBuffer;
	int _indexCount = 0;

	void CreateSphereMesh(float radius, int sliceCount, int stackCount,
		std::vector<Vertex>& vertices, std::vector<UINT>& indices);
	float _radius = 0.5f;
	int _stackCount = 16;
	int _sliceCount = 16;
	void RebuildBuffers();
	void UpdateBuffers();
public:

	ComPtr<ID3D11ShaderResourceView> _texture = nullptr;

	void Awake() override;
	void Render()const override;

	void ShowUI() override;
	float GetRadius()const { return _radius; }
	std::string GetComponentName() override { return "Sphere"; }

	template <class Archive>
	void serialize(Archive& ar) {
		ar(cereal::base_class<Component>(this));
		ar(cereal::make_nvp("Radius", _radius));
		ar(cereal::make_nvp("StackCount", _stackCount));
		ar(cereal::make_nvp("SliceCount", _sliceCount));
	}

};


#endif // !_SPHERE_H_