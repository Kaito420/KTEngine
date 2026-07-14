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
	int _shadingModelID = 0; // 0: Smooth, 1: Toon
	bool _flipU = false;
	bool _flipV = false;

	// Texture Maps
	std::string _texturePath = "";
	const TEXTURE* _texture = nullptr;

	std::string _normalMapPath = "";
	const TEXTURE* _normalMap = nullptr;

	std::string _metallicMapPath = "";
	const TEXTURE* _metallicMap = nullptr;

	std::string _roughnessMapPath = "";
	const TEXTURE* _roughnessMap = nullptr;

	std::string _armMapPath = "";
	const TEXTURE* _armMap = nullptr;

public:
	void Awake() override;
    void SetVertexShader(std::string id);
    void SetPixelShader(std::string id);
	std::string GetVertexShaderID() const { return _vertexShaderID; }
	std::string GetVertexLayoutID() const { return _vertexLayoutID; }
	std::string GetPixelShaderID() const { return _pixelShaderID; }

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
	bool GetFlipU() const { return _flipU; }
	void SetFlipU(bool val) { _flipU = val; }
	bool GetFlipV() const { return _flipV; }
	void SetFlipV(bool val) { _flipV = val; }

	// BaseColor Texture
	const TEXTURE* GetTexture() const { return _texture; }
	std::string GetTexturePath() const { return _texturePath; }
	void SetTexturePath(const std::string& path);

	// Normal Map
	const TEXTURE* GetNormalMap() const { return _normalMap; }
	std::string GetNormalMapPath() const { return _normalMapPath; }
	void SetNormalMapPath(const std::string& path);

	// Metallic Map
	const TEXTURE* GetMetallicMap() const { return _metallicMap; }
	std::string GetMetallicMapPath() const { return _metallicMapPath; }
	void SetMetallicMapPath(const std::string& path);

	// Roughness Map
	const TEXTURE* GetRoughnessMap() const { return _roughnessMap; }
	std::string GetRoughnessMapPath() const { return _roughnessMapPath; }
	void SetRoughnessMapPath(const std::string& path);

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
		ar(cereal::make_nvp("MetallicMapPath", _metallicMapPath));
		ar(cereal::make_nvp("RoughnessMapPath", _roughnessMapPath));
		try {
			ar(cereal::make_nvp("ARMMapPath", _armMapPath));
		} catch (...) {
			_armMapPath = "";
		}

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
		if (!_metallicMapPath.empty()) {
			_metallicMap = Texture::Load(_metallicMapPath.c_str());
		} else {
			_metallicMap = nullptr;
		}
		if (!_roughnessMapPath.empty()) {
			_roughnessMap = Texture::Load(_roughnessMapPath.c_str());
		} else {
			_roughnessMap = nullptr;
		}
		if (!_armMapPath.empty()) {
			_armMap = Texture::Load(_armMapPath.c_str());
		} else {
			_armMap = nullptr;
		}
	}
};


#endif // !_SHADER_H