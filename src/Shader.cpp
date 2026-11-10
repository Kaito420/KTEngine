//=====================================================================================
// Shader.cpp
// Author:Kaito Aoki
// Date:2026/01/29
//=====================================================================================

#include "Shader.h"
#include "Renderer.h"
#include "ShaderManager.h"
#include "HotReloadManager.h"
#include "Texture.h"
#include "Manager.h"
#include <filesystem>
#include <vector>
#include <algorithm>

void Shader::Awake(){
	// デフォルトシェーダー設定
	if (_vertexShaderID.empty()) SetVertexShader("DirectionalLight");
	if (_pixelShaderID.empty()) SetPixelShader("DirectionalLight");
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
	_mappingMode = source->_mappingMode;
	_flipU = source->_flipU;
	_flipV = source->_flipV;
	_uvTiling = source->_uvTiling;
	_uvOffset = source->_uvOffset;
	_uvWrapMode = source->_uvWrapMode;
	_uvScaleMode = source->_uvScaleMode;
	_procScale3D = source->_procScale3D;
	_procSeed = source->_procSeed;

	SetTexturePath(source->_texturePath);
	SetNormalMapPath(source->_normalMapPath);
	SetARMMapPath(source->_armMapPath);
}

void Shader::FillMaterialStruct(MATERIAL& material) const {
	material.EmissionColor = _emissionColor;
	material.Metallic = _metallic;
	material.SpecularPbr = _specular;
	material.Roughness = _roughness;
	material.NormalWeight = _normalWeight;
	material.ShadingModelID = _shadingModelID;
	material.FlipU = _flipU ? 1 : 0;
	material.FlipV = _flipV ? 1 : 0;
	material.HasNormalMap = _normalMap ? 1 : 0;
	material.HasMetallicMap = _armMap ? 1 : 0;
	material.HasRoughnessMap = _mappingMode;

	// Pack UE5-style UV Tiling & Offset into Ambient (xy = Tiling, zw = Offset)
	material.Ambient = { _uvTiling.x, _uvTiling.y, _uvOffset.x, _uvOffset.y };
	// Pack WrapMode, UVScaleMode, Scale3D, Seed3D into Specular (SpecularOld in HLSL)
	material.Specular = {
		static_cast<float>(_uvWrapMode),
		static_cast<float>(_uvScaleMode),
		_procScale3D,
		static_cast<float>(_procSeed)
	};
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

// shader/ フォルダ内のプロシージャル PixelShader (Proc*PS.hlsl) 一覧取得
static std::vector<std::string> GetAvailableProceduralPixelShaders() {
	std::vector<std::string> list;
	namespace fs = std::filesystem;
	std::error_code ec;
	if (fs::exists("shader", ec) && fs::is_directory("shader", ec)) {
		for (const auto& entry : fs::directory_iterator("shader", ec)) {
			if (!entry.is_regular_file(ec)) continue;
			std::string ext = entry.path().extension().string();
			if (_stricmp(ext.c_str(), ".hlsl") != 0) continue;
			std::string stem = entry.path().stem().string();
			if (stem.size() >= 6 && _strnicmp(stem.c_str(), "Proc", 4) == 0 &&
			    _stricmp(stem.c_str() + stem.size() - 2, "PS") == 0) {
				list.push_back(stem);
			}
		}
	}
	std::sort(list.begin(), list.end());
	return list;
}

void Shader::ShowUI(){
	ImGui::Text("Active VS: %s | Active PS: %s",
		_vertexShaderID.c_str(),
		(_mappingMode == 3 && _pixelShaderID.size() >= 4 && _strnicmp(_pixelShaderID.c_str(), "Proc", 4) == 0)
			? _pixelShaderID.c_str()
			: "GeometryPS");

	// 1. Common Material Properties
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

	// 2. Texture & Mapping Mode Selection
	ImGui::Separator();
	ImGui::Text("Mapping & UV Controls (UE5 Style):");

	const char* mappingModes[] = {
		"2D Standard (Mesh UV)",
		"3D Solid PNG (Object Spherical / Box)",
		"3D World Triplanar PNG (No-Stretch / Random)",
		"3D Procedural HLSL (Real-Time PixelShader)"
	};
	int prevMode = _mappingMode;
	if (ImGui::Combo("Texture Mapping", &_mappingMode, mappingModes, IM_ARRAYSIZE(mappingModes))) {
		if (_mappingMode != 3 && prevMode == 3) {
			if (_pixelShaderID.size() >= 4 && _strnicmp(_pixelShaderID.c_str(), "Proc", 4) == 0) {
				SetPixelShader("DirectionalLight");
			}
		}
	}

	// 3. Mode-Adaptive Sub-Panel (Inspector UI Plan 1)
	if (_mappingMode == 0) {
		// --- Mode 0: 2D Standard (Mesh UV) ---
		const char* scaleBehaviors2D[] = {
			"Stretch with Object (Default UV)",
			"Auto-Loop by Object Scale (UE5 World-Aligned)"
		};
		ImGui::Combo("Object Scale Behavior", &_uvScaleMode, scaleBehaviors2D, IM_ARRAYSIZE(scaleBehaviors2D));

		const char* wrapModes[] = {
			"Repeat (Loop)",
			"Clamp (Stretch Edge)",
			"Mirror (Ping-Pong)",
			"Stochastic Random Tile (No-Repeat)"
		};
		ImGui::Combo("UV Wrap Mode", &_uvWrapMode, wrapModes, IM_ARRAYSIZE(wrapModes));

		float tiling[2] = { _uvTiling.x, _uvTiling.y };
		if (ImGui::DragFloat2("UV Tiling (U, V)", tiling, 0.05f, 0.01f, 100.0f, "%.2f")) {
			_uvTiling = { tiling[0], tiling[1] };
		}
		float offset[2] = { _uvOffset.x, _uvOffset.y };
		if (ImGui::DragFloat2("UV Offset (U, V)", offset, 0.01f, -50.0f, 50.0f, "%.2f")) {
			_uvOffset = { offset[0], offset[1] };
		}
		if (ImGui::SmallButton("Reset UV Tiling/Offset")) {
			_uvTiling = { 1.0f, 1.0f };
			_uvOffset = { 0.0f, 0.0f };
		}
		if (_uvWrapMode == 3) {
			ImGui::DragInt("Stochastic Seed", &_procSeed, 1.0f, 1, 999999);
		}
		ImGui::Checkbox("Flip UV U", &_flipU);
		ImGui::SameLine();
		ImGui::Checkbox("Flip UV V", &_flipV);

		ImGui::Separator();
		ImGui::Text("Texture Maps (PNG / DDS):");
		ShowTextureCombo("BaseColor Map", _texturePath, [this](const std::string& p) { SetTexturePath(p); });
		ShowTextureCombo("Normal Map", _normalMapPath, [this](const std::string& p) { SetNormalMapPath(p); });
		ShowTextureCombo("ARM Map", _armMapPath, [this](const std::string& p) { SetARMMapPath(p); });
	}
	else if (_mappingMode == 1) {
		// --- Mode 1: 3D Solid PNG (Object Spherical / Box) ---
		float tiling[2] = { _uvTiling.x, _uvTiling.y };
		if (ImGui::DragFloat2("Solid Tiling (U, V)", tiling, 0.05f, 0.01f, 50.0f, "%.2f")) {
			_uvTiling = { tiling[0], tiling[1] };
		}
		float offset[2] = { _uvOffset.x, _uvOffset.y };
		if (ImGui::DragFloat2("Solid Offset (U, V)", offset, 0.01f, -10.0f, 10.0f, "%.2f")) {
			_uvOffset = { offset[0], offset[1] };
		}
		ImGui::Checkbox("Flip UV U", &_flipU);
		ImGui::SameLine();
		ImGui::Checkbox("Flip UV V", &_flipV);

		ImGui::Separator();
		ImGui::Text("3D Solid Panorama Maps (PNG):");
		ShowTextureCombo("BaseColor Map", _texturePath, [this](const std::string& p) { SetTexturePath(p); });
		ShowTextureCombo("Normal Map", _normalMapPath, [this](const std::string& p) { SetNormalMapPath(p); });
		ShowTextureCombo("ARM Map", _armMapPath, [this](const std::string& p) { SetARMMapPath(p); });
	}
	else if (_mappingMode == 2) {
		// --- Mode 2: 3D World Triplanar PNG (No-Stretch / Random Tile) ---
		const char* spaceModes3D[] = {
			"Auto-Scale with Object (No Stretch)",
			"Normalized Object Space [-1, 1]"
		};
		ImGui::Combo("3D Coordinate Space", &_uvScaleMode, spaceModes3D, IM_ARRAYSIZE(spaceModes3D));

		const char* wrapModes3D[] = {
			"Repeat (Standard Triplanar)",
			"Clamp (Stretch Edge)",
			"Mirror (Ping-Pong)",
			"Stochastic 3D Noise Anti-Tile (No-Repeat)"
		};
		ImGui::Combo("Triplanar Wrap Mode", &_uvWrapMode, wrapModes3D, IM_ARRAYSIZE(wrapModes3D));

		ImGui::SliderFloat("3D Tile Density", &_procScale3D, 0.05f, 20.0f, "%.2f");
		float tiling[2] = { _uvTiling.x, _uvTiling.y };
		if (ImGui::DragFloat2("Fine Tiling (U, V)", tiling, 0.05f, 0.01f, 50.0f, "%.2f")) {
			_uvTiling = { tiling[0], tiling[1] };
		}
		float offset[2] = { _uvOffset.x, _uvOffset.y };
		if (ImGui::DragFloat2("3D Offset (X, Y)", offset, 0.01f, -50.0f, 50.0f, "%.2f")) {
			_uvOffset = { offset[0], offset[1] };
		}
		if (_uvWrapMode == 3) {
			ImGui::DragInt("3D Noise Seed", &_procSeed, 1.0f, 1, 999999);
		}

		ImGui::Separator();
		ImGui::Text("Triplanar Texture Maps (PNG):");
		ShowTextureCombo("BaseColor Map", _texturePath, [this](const std::string& p) { SetTexturePath(p); });
		ShowTextureCombo("Normal Map", _normalMapPath, [this](const std::string& p) { SetNormalMapPath(p); });
		ShowTextureCombo("ARM Map", _armMapPath, [this](const std::string& p) { SetARMMapPath(p); });
	}
	else if (_mappingMode == 3) {
		// --- Mode 3: 3D Procedural HLSL (Real-Time PixelShader) ---
		ImGui::Separator();
		ImGui::TextColored(ImVec4(0.45f, 0.88f, 1.0f, 1.0f), "Real-Time 3D Procedural PixelShader (Infinite Res):");

		auto procShaders = GetAvailableProceduralPixelShaders();
		std::string currentLabel = "Built-in 3D Voxel Grid & FBM (GeometryPS)";
		if (_pixelShaderID.size() >= 4 && _strnicmp(_pixelShaderID.c_str(), "Proc", 4) == 0) {
			currentLabel = _pixelShaderID + ".hlsl";
		}

		if (ImGui::BeginCombo("Procedural Shader (.hlsl)", currentLabel.c_str())) {
			bool isBuiltIn = (_pixelShaderID.size() < 4 || _strnicmp(_pixelShaderID.c_str(), "Proc", 4) != 0);
			if (ImGui::Selectable("Built-in 3D Voxel Grid & FBM (GeometryPS)", isBuiltIn)) {
				SetPixelShader("Geometry");
			}
			for (const auto& stem : procShaders) {
				bool selected = (_pixelShaderID == stem);
				std::string itemLabel = stem + ".hlsl (shader/" + stem + ".hlsl)";
				if (ImGui::Selectable(itemLabel.c_str(), selected)) {
					SetPixelShader(stem);
					std::string err;
					ShaderManager::Instance().CompileAndRegisterPixelShaderHLSL(stem, std::filesystem::path("shader") / (stem + ".hlsl"), err);
				}
				if (selected) ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}

		const char* spaceModes3D[] = {
			"Auto-Scale with Object (No Stretch)",
			"Normalized Object Space [-1, 1]"
		};
		ImGui::Combo("3D Coordinate Space", &_uvScaleMode, spaceModes3D, IM_ARRAYSIZE(spaceModes3D));

		ImGui::SliderFloat("3D Pattern Density", &_procScale3D, 0.05f, 20.0f, "%.2f");
		float offset[2] = { _uvOffset.x, _uvOffset.y };
		if (ImGui::DragFloat2("3D Axis Offset (X, Z)", offset, 0.02f, -50.0f, 50.0f, "%.2f")) {
			_uvOffset = { offset[0], offset[1] };
		}
		ImGui::DragInt("3D Noise Seed", &_procSeed, 1.0f, 1, 999999);

		if (ImGui::Button("Recompile Procedural HLSL")) {
			std::string err;
			if (_pixelShaderID.size() >= 4 && _strnicmp(_pixelShaderID.c_str(), "Proc", 4) == 0) {
				std::filesystem::path p = std::filesystem::path("shader") / (_pixelShaderID + ".hlsl");
				ShaderManager::Instance().CompileAndRegisterPixelShaderHLSL(_pixelShaderID, p, err);
			} else {
				HotReloadManager::ForceRecompileAllShaders();
			}
		}
	}

	ImGui::Separator();
	if (ImGui::Button("Open Procedural Texture Editor...")) {
		Manager::GetShowProceduralTextureEditor() = true;
	}
}
