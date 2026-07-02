//=====================================================================================
// Shader.cpp
// Author:Kaito Aoki
// Date:2026/01/29
//=====================================================================================

#include "Shader.h"
#include "Renderer.h"
#include "ShaderManager.h"

void Shader::Awake(){
	//デフォルトシェーダー設定
	SetVertexShader("DirectionalLight");
	SetPixelShader("DirectionalLight");
}

void Shader::SetVertexShader(std::string id){
	_vertexShaderID = id;
	_vertexLayoutID = id;
}

void Shader::SetPixelShader(std::string id){
	_pixelShaderID = id;
}

void Shader::ShowUI(){
	ImGui::Text("Shader ID: %s", _vertexShaderID.c_str());

	// Material properties editing controls
	ImGui::Separator();
	ImGui::Text("Material Settings:");
	
	float color[4] = { _baseColor.x, _baseColor.y, _baseColor.z, _baseColor.w };
	if (ImGui::ColorEdit4("Base Color", color)) {
		_baseColor = { color[0], color[1], color[2], color[3] };
	}
	
	float emission[4] = { _emissionColor.x, _emissionColor.y, _emissionColor.z, _emissionColor.w };
	if (ImGui::ColorEdit4("Emission Color", emission)) {
		_emissionColor = { emission[0], emission[1], emission[2], emission[3] };
	}
	
	ImGui::SliderFloat("Metallic", &_metallic, 0.0f, 1.0f);
	ImGui::SliderFloat("Specular", &_specular, 0.0f, 1.0f);
	ImGui::SliderFloat("Roughness", &_roughness, 0.0f, 1.0f);
	ImGui::SliderFloat("Normal Weight", &_normalWeight, 0.0f, 1.0f);
	
	const char* shadingModels[] = { "Smooth", "Toon" };
	ImGui::Combo("Shading Model", &_shadingModelID, shadingModels, IM_ARRAYSIZE(shadingModels));
}
