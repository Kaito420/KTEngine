//=====================================================================================
// Shader.cpp
// Author:Kaito Aoki
// Date:2026/01/29
//=====================================================================================

#include "Shader.h"
#include "Renderer.h"
#include "ShaderManager.h"
#include "Texture.h"
#include <filesystem>
#include <vector>

void Shader::Awake(){
	//デフォルトシェーダー設定
	SetVertexShader("DirectionalLight");
	SetPixelShader("DirectionalLight");
	if (!_texturePath.empty()) {
		_texture = Texture::Load(_texturePath.c_str());
	}
	if (!_normalMapPath.empty()) {
		_normalMap = Texture::Load(_normalMapPath.c_str());
	}
	if (!_metallicMapPath.empty()) {
		_metallicMap = Texture::Load(_metallicMapPath.c_str());
	}
	if (!_roughnessMapPath.empty()) {
		_roughnessMap = Texture::Load(_roughnessMapPath.c_str());
	}
}

void Shader::SetVertexShader(std::string id){
	_vertexShaderID = id;
	_vertexLayoutID = id;
}

void Shader::SetPixelShader(std::string id){
	_pixelShaderID = id;
}

void Shader::SetTexturePath(const std::string& path) {
	_texturePath = path;
	if (!_texturePath.empty()) {
		_texture = Texture::Load(_texturePath.c_str());
	} else {
		_texture = nullptr;
	}
}

void Shader::SetNormalMapPath(const std::string& path) {
	_normalMapPath = path;
	if (!_normalMapPath.empty()) {
		_normalMap = Texture::Load(_normalMapPath.c_str());
	} else {
		_normalMap = nullptr;
	}
}

void Shader::SetMetallicMapPath(const std::string& path) {
	_metallicMapPath = path;
	if (!_metallicMapPath.empty()) {
		_metallicMap = Texture::Load(_metallicMapPath.c_str());
	} else {
		_metallicMap = nullptr;
	}
}

void Shader::SetRoughnessMapPath(const std::string& path) {
	_roughnessMapPath = path;
	if (!_roughnessMapPath.empty()) {
		_roughnessMap = Texture::Load(_roughnessMapPath.c_str());
	} else {
		_roughnessMap = nullptr;
	}
}

// テクスチャ選択UIの共通ヘルパー
static void ShowTextureCombo(const char* label, std::string& currentPath, 
	std::function<void(const std::string&)> setter) {
	std::string currentTexName = currentPath.empty() ? "None" : std::filesystem::path(currentPath).filename().string();
	if (ImGui::BeginCombo(label, currentTexName.c_str())) {
		if (ImGui::Selectable("None", currentPath.empty())) {
			setter("");
		}
		std::vector<std::string> texDirs = { "asset/texture", "asset/model" };
		for (const auto& dir : texDirs) {
			if (std::filesystem::exists(dir) && std::filesystem::is_directory(dir)) {
				for (const auto& entry : std::filesystem::directory_iterator(dir)) {
					if (entry.is_regular_file()) {
						std::filesystem::path filePath = entry.path();
						std::string ext = filePath.extension().string();
						if (ext == ".png" || ext == ".PNG" || ext == ".jpg" || ext == ".JPG" || ext == ".jpeg" || ext == ".tga" || ext == ".dds") {
							std::string filename = filePath.filename().string();
							std::string relativePath = filePath.generic_string();

							bool isSelected = (currentPath == relativePath);
							if (ImGui::Selectable((filename + " (" + dir + ")").c_str(), isSelected)) {
								setter(relativePath);
							}
							if (isSelected) {
								ImGui::SetItemDefaultFocus();
							}
						}
					}
				}
			}
		}
		ImGui::EndCombo();
	}
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

	ImGui::Checkbox("Flip UV U", &_flipU);
	ImGui::SameLine();
	ImGui::Checkbox("Flip UV V", &_flipV);

	// Texture Maps
	ImGui::Separator();
	ImGui::Text("Texture Maps:");

	ShowTextureCombo("BaseColor Map", _texturePath, [this](const std::string& p) { SetTexturePath(p); });
	ShowTextureCombo("Normal Map", _normalMapPath, [this](const std::string& p) { SetNormalMapPath(p); });
	ShowTextureCombo("Metallic Map", _metallicMapPath, [this](const std::string& p) { SetMetallicMapPath(p); });
	ShowTextureCombo("Roughness Map", _roughnessMapPath, [this](const std::string& p) { SetRoughnessMapPath(p); });
}
