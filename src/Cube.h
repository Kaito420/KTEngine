//=====================================================================================
// Cube.h
// Author:Kaito Aoki
// Date:2025/09/08
//=====================================================================================

#ifndef _CUBE_H_
#define _CUBE_H_

#include "Component.h"
#include "RendererDX11.h"
#include <cereal/types/base_class.hpp>
#include <cereal/types/polymorphic.hpp>

class Cube : public Component{
	friend class cereal::access;
private:
	ComPtr<ID3D11Buffer> _vertexBuffer;
	ComPtr<ID3D11Buffer> _indexBuffer;

public:

	void Awake() override;
	void Render()const override;

	void ShowUI() override;

	std::string GetComponentName()override { return "Cube"; }

	template <class Archive>
	void serialize(Archive& ar) {
		ar(cereal::base_class<Component>(this));
	}
};



#endif// !_CUBE_H_