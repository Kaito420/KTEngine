//=====================================================================================
// Shader.h
// Author:Kaito Aoki
// Date:2026/01/29
//=====================================================================================

#ifndef _SHADER_H
#define _SHADER_H

#include "Component.h"
#include "Renderer.h"
#include "Texture.h"
#include <string>
#include <cereal/types/base_class.hpp>
#include <cereal/types/polymorphic.hpp>
#include "ktvector.hpp"

class Shader : public Component {
	friend class cereal::access;
private:
	std::string _vertexShaderID;
	std::string	_vertexLayoutID;
	std::string _pixelShaderID;

	// Material properties for Deferred Rendering
	XMFLOAT4 _baseColor = { 1.0f, 1.0f, 1.0f, 1.0f };
	XMFLOAT4 _emissionColor = { 0.0f, 0.0f, 0.0f, 0.0f };
	float _metallic = 0.0f;
	float _specular = 0.5f;
	float _roughness = 0.5f;
	float _normalWeight = 1.0f;
	int _shadingModelID = 0; // 0: Smooth, 1: Toon, 2: Ambient Only, 3: Unlit
	int _mappingMode = 0;    // 0: 2D Standard, 1: 3D Solid PNG, 2: 3D World Triplanar PNG, 3: 3D Procedural HLSL
	bool _flipU = false;
	bool _flipV = false;

	// UE5-Style 2D UV & 3D Procedural Mapping Controls
	XMFLOAT2 _uvTiling = { 1.0f, 1.0f };
	XMFLOAT2 _uvOffset = { 0.0f, 0.0f };
	int _uvWrapMode = 0;     // 0: Repeat (Loop), 1: Clamp (Stretch Edge), 2: Mirror (Ping-Pong), 3: Stochastic Random Tile
	int _uvScaleMode = 0;    // 2D: (0: Stretch with Object, 1: Auto-Loop by Object Scale), 3D: (0: Auto-Scale with Object, 1: Normalized [-1,1])
	float _procScale3D = 1.0f;
	int _procSeed = 1337;

	// Texture Maps
	std::string _texturePath = "";
	const TEXTURE* _texture = nullptr;

	std::string _normalMapPath = "";
	const TEXTURE* _normalMap = nullptr;

	std::string _armMapPath = "";
	const TEXTURE* _armMap = nullptr;

public:
	void Awake() override;
	void SetVertexShader(std::string id);
	void SetPixelShader(std::string id);
	std::string GetVertexShaderID() const { return _vertexShaderID; }
	std::string GetVertexLayoutID() const { return _vertexLayoutID; }
	std::string GetPixelShaderID() const { return _pixelShaderID; }

	// Copy all shader & material properties from source Shader
	void CopyFrom(const Shader* source);

	// Populate MATERIAL constant buffer structure with all PBR, UV, and 3D parameters
	void FillMaterialStruct(MATERIAL& material) const;

	// Getters and Setters for Material
	XMFLOAT4 GetBaseColor() const { return _baseColor; }
	void SetBaseColor(const XMFLOAT4& val) { _baseColor = val; }
	XMFLOAT4 GetEmissionColor() const { return _emissionColor; }
	void SetEmissionColor(const XMFLOAT4& val) { _emissionColor = val; }
	float GetMetallic() const { return _metallic; }
	void SetMetallic(float val) { _metallic = val; }
	float GetSpecular() const { return _specular; }
	void SetSpecular(float val) { _specular = val; }
	float GetRoughness() const { return _roughness; }
	void SetRoughness(float val) { _roughness = val; }
	float GetNormalWeight() const { return _normalWeight; }
	void SetNormalWeight(float val) { _normalWeight = val; }
	int GetShadingModelID() const { return _shadingModelID; }
	void SetShadingModelID(int val) { _shadingModelID = val; }
	int GetMappingMode() const { return _mappingMode; }
	void SetMappingMode(int val) { _mappingMode = val; }
	bool GetFlipU() const { return _flipU; }
	void SetFlipU(bool val) { _flipU = val; }
	bool GetFlipV() const { return _flipV; }
	void SetFlipV(bool val) { _flipV = val; }

	XMFLOAT2 GetUVTiling() const { return _uvTiling; }
	void SetUVTiling(const XMFLOAT2& val) { _uvTiling = val; }
	XMFLOAT2 GetUVOffset() const { return _uvOffset; }
	void SetUVOffset(const XMFLOAT2& val) { _uvOffset = val; }
	int GetUVWrapMode() const { return _uvWrapMode; }
	void SetUVWrapMode(int val) { _uvWrapMode = val; }
	int GetUVScaleMode() const { return _uvScaleMode; }
	void SetUVScaleMode(int val) { _uvScaleMode = val; }
	float GetProcScale3D() const { return _procScale3D; }
	void SetProcScale3D(float val) { _procScale3D = val; }
	int GetProcSeed() const { return _procSeed; }
	void SetProcSeed(int val) { _procSeed = val; }

	// BaseColor Texture
	const TEXTURE* GetTexture() const { return _texture; }
	std::string GetTexturePath() const { return _texturePath; }
	void SetTexturePath(const std::string& path);

	// Normal Map
	const TEXTURE* GetNormalMap() const { return _normalMap; }
	std::string GetNormalMapPath() const { return _normalMapPath; }
	void SetNormalMapPath(const std::string& path);

	// ARM Map
	const TEXTURE* GetARMMap() const { return _armMap; }
	std::string GetARMMapPath() const { return _armMapPath; }
	void SetARMMapPath(const std::string& path);

	void ShowUI() override;
	std::string GetComponentName() override { return "Shader"; }

	template <class Archive>
	void serialize(Archive& ar) {
		ar(cereal::base_class<Component>(this));
		ar(cereal::make_nvp("VertexShaderID", _vertexShaderID));
		ar(cereal::make_nvp("VertexLayoutID", _vertexLayoutID));
		ar(cereal::make_nvp("PixelShaderID", _pixelShaderID));
		ar(cereal::make_nvp("BaseColor", _baseColor));
		ar(cereal::make_nvp("EmissionColor", _emissionColor));
		ar(cereal::make_nvp("Metallic", _metallic));
		ar(cereal::make_nvp("Specular", _specular));
		ar(cereal::make_nvp("Roughness", _roughness));
		ar(cereal::make_nvp("NormalWeight", _normalWeight));
		ar(cereal::make_nvp("ShadingModelID", _shadingModelID));
		ar(cereal::make_nvp("FlipU", _flipU));
		ar(cereal::make_nvp("FlipV", _flipV));
		ar(cereal::make_nvp("TexturePath", _texturePath));
		ar(cereal::make_nvp("NormalMapPath", _normalMapPath));
		std::string dummyMetallicPath = "";
		std::string dummyRoughnessPath = "";
		try { ar(cereal::make_nvp("MetallicMapPath", dummyMetallicPath)); } catch (...) {}
		try { ar(cereal::make_nvp("RoughnessMapPath", dummyRoughnessPath)); } catch (...) {}
		try { ar(cereal::make_nvp("ARMMapPath", _armMapPath)); } catch (...) { _armMapPath = ""; }
		try { ar(cereal::make_nvp("MappingMode", _mappingMode)); } catch (...) { _mappingMode = 0; }
		try { ar(cereal::make_nvp("UVTilingX", _uvTiling.x)); } catch (...) { _uvTiling.x = 1.0f; }
		try { ar(cereal::make_nvp("UVTilingY", _uvTiling.y)); } catch (...) { _uvTiling.y = 1.0f; }
		try { ar(cereal::make_nvp("UVOffsetX", _uvOffset.x)); } catch (...) { _uvOffset.x = 0.0f; }
		try { ar(cereal::make_nvp("UVOffsetY", _uvOffset.y)); } catch (...) { _uvOffset.y = 0.0f; }
		try { ar(cereal::make_nvp("UVWrapMode", _uvWrapMode)); } catch (...) { _uvWrapMode = 0; }
		try { ar(cereal::make_nvp("UVScaleMode", _uvScaleMode)); } catch (...) { _uvScaleMode = 0; }
		try { ar(cereal::make_nvp("ProcScale3D", _procScale3D)); } catch (...) { _procScale3D = 1.0f; }
		try { ar(cereal::make_nvp("ProcSeed", _procSeed)); } catch (...) { _procSeed = 1337; }

		if (!_texturePath.empty()) {
			_texture = Texture::Load(_texturePath.c_str());
		} else {
			_texture = nullptr;
		}
		if (!_normalMapPath.empty()) {
			_normalMap = Texture::Load(_normalMapPath.c_str());
		} else {
			_normalMap = nullptr;
		}
		if (!_armMapPath.empty()) {
			_armMap = Texture::Load(_armMapPath.c_str());
		} else {
			_armMap = nullptr;
		}
	}
};

#endif // !_SHADER_H
