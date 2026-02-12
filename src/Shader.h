//=====================================================================================
// Shader.h
// Author:Kaito Aoki
// Date:2026/01/29
//=====================================================================================

#ifndef _SHADER_H
#define _SHADER_H

#include "Component.h"
#include "RendererDX11.h"
#include <string>
#include <cereal/types/base_class.hpp>
#include <cereal/types/polymorphic.hpp>

class Shader : public Component {
	friend class cereal::access;
private:
	std::string _vertexShaderID;
	std::string	_vertexLayoutID;
	std::string _pixelShaderID;

public:
	void Awake() override;
    void SetVertexShader(std::string id);
    void SetPixelShader(std::string id);
	std::string GetVertexShaderID() const { return _vertexShaderID; }
	std::string GetVertexLayoutID() const { return _vertexLayoutID; }
	std::string GetPixelShaderID() const { return _pixelShaderID; }

	void ShowUI() override;
	std::string GetComponentName() override { return "Shader"; }

	template <class Archive>
	void serialize(Archive& ar) {
		ar(cereal::base_class<Component>(this));
		ar(cereal::make_nvp("VertexShaderID", _vertexShaderID));
		ar(cereal::make_nvp("VertexLayoutID", _vertexLayoutID));
		ar(cereal::make_nvp("PixelShaderID", _pixelShaderID));
	}
};


#endif // !_SHADER_H