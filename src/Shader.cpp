//=====================================================================================
// Shader.cpp
// Author:Kaito Aoki
// Date:2026/01/29
//=====================================================================================

#include "Shader.h"
#include "RendererDX11.h"
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
	//シェーダーidを表示する
	ImGui::Text("Shader ID: %s", _vertexShaderID.c_str());
}

