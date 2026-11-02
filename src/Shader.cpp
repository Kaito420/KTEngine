//=====================================================================================
// Shader.cpp
// Author:Kaito Aoki
// Date:2026/01/29
//=====================================================================================

#include "Shader.h"
#include "Renderer.h"
#include "ShaderManager.h"
#include "Texture.h"
#include "Manager.h"
#include <filesystem>
#include <vector>
#include <algorithm>

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
	if (!_armMapPath.empty()) {
		_armMap = Texture::Load(_armMapPath.c_str());
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

void Shader::SetARMMapPath(const std::string& path) {
	_armMapPath = path;
	if (!_armMapPath.empty()) {
		_armMap = Texture::Load(_armMapPath.c_str());
	} else {
		_armMap = nullptr;
	}
}

void Shader::CopyFrom(const Shader* source) {
	if (!source) return;
	_vertexShaderID = source->_vertexShaderID;
	_vertexLayoutID = source->_vertexLayoutID;
	_pixelShaderID = source->_pixelShaderID;
	_baseColor = source->_baseColor;
	_emissionColor = source->_emissionColor;
	_metallic = source->_metallic;
	_specular = source->_specular;
	_roughness = source->_roughness;
	_normalWeight = source->_normalWeight;
	_shadingModelID = source->_shadingModelID;
	_flipU = source->_flipU;
	_flipV = source->_flipV;

	SetTexturePath(source->_texturePath);
	SetNormalMapPath(source->_normalMapPath);
	SetARMMapPath(source->_armMapPath);
}

// 対応テクスチャ拡張子判定（大小文字区別なし）
static bool IsSupportedTextureExtension(const std::filesystem::path& path) {
	std::string ext = path.extension().string();
	std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
	return ext == ".png" || ext == ".jpg" || ext == ".jpeg" ||
	       ext == ".bmp" || ext == ".tga" || ext == ".dds";
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
				std::error_code ec;
				for (const auto& entry : std::filesystem::recursive_directory_iterator(dir, std::filesystem::directory_options::skip_permission_denied, ec)) {
					if (entry.is_regular_file()) {
						std::filesystem::path filePath = entry.path();
						if (IsSupportedTextureExtension(filePath)) {
							std::string filename = filePath.filename().string();
							std::string relativePath = filePath.generic_string();

							bool isSelected = (currentPath == relativePath);
							std::string parentDir = filePath.parent_path().generic_string();
							if (ImGui::Selectable((filename + " (" + parentDir + ")").c_str(), isSelected)) {
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

	// FileBrowser（Content Browser）からのドラッグ＆ドロップ受け入れ
	if (ImGui::BeginDragDropTarget()) {
		const ImGuiPayload* payload = ImGui::GetDragDropPayload();
		if (payload && payload->IsDataType("DND_ASSET_PATH") && payload->Data != nullptr) {
			std::string previewPath(static_cast<const char*>(payload->Data));
			if (IsSupportedTextureExtension(previewPath)) {
				if (const ImGuiPayload* accepted = ImGui::AcceptDragDropPayload("DND_ASSET_PATH")) {
					std::string droppedPath(static_cast<const char*>(accepted->Data));
					setter(droppedPath);
				}
			}
		}
		ImGui::EndDragDropTarget();
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
	
	const char* shadingModels[] = { "Smooth", "Toon", "Ambient Only", "Unlit" };
	ImGui::Combo("Shading Model", &_shadingModelID, shadingModels, IM_ARRAYSIZE(shadingModels));

	ImGui::Checkbox("Flip UV U", &_flipU);
	ImGui::SameLine();
	ImGui::Checkbox("Flip UV V", &_flipV);

	// Texture Maps
	ImGui::Separator();
	ImGui::Text("Texture Maps:");

	ShowTextureCombo("BaseColor Map", _texturePath, [this](const std::string& p) { SetTexturePath(p); });
	ShowTextureCombo("Normal Map", _normalMapPath, [this](const std::string& p) { SetNormalMapPath(p); });
	ShowTextureCombo("ARM Map", _armMapPath, [this](const std::string& p) { SetARMMapPath(p); });
	if (ImGui::Button("Open Procedural Texture Editor...")) {
		Manager::GetShowProceduralTextureEditor() = true;
	}
}
