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

class Shader : public Component {
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
};


#endif // !_SHADER_H