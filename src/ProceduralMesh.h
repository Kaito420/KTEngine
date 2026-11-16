//=====================================================================================
// ProceduralMesh.h
// Author: KTEngine
// Description: Base & Field Procedural Mesh Component.
//              Defaults to a subdivided Flat Plane and supports PNG Displacement Mapping,
//              3D/2D Procedural Noise (Perlin FBM / Ridged / Terrace), and virtual hooks
//              for derived procedural field classes. Fully integrated with ColliderMesh.
//=====================================================================================

#ifndef _PROCEDURAL_MESH_H_
#define _PROCEDURAL_MESH_H_

#include "Component.h"
#include "Renderer.h"
#include "ktvector.hpp"
#include <vector>
#include <string>
#include <cstdint>
#include <cereal/types/base_class.hpp>
#include <cereal/types/polymorphic.hpp>
#include <cereal/types/string.hpp>

class ProceduralMesh : public Component {
	friend class cereal::access;

public:
	enum class GeneratorMode : int {
		FlatPlane           = 0, // Default flat subdivided plane
		DisplacementMap     = 1, // Height driven by attached PNG/TGA/DDS grayscale luminance
		ProceduralNoise     = 2, // Height driven by mathematical noise (Perlin FBM / Ridged / Terrace)
		CombinedMapAndNoise = 3  // Macro PNG Displacement + Micro Procedural Noise
	};

	enum class NoiseType : int {
		PerlinFBM         = 0,
		RidgedMountains   = 1,
		TerracedField     = 2,
		SineWaveRipples   = 3
	};

protected:
	// Base Grid / Plane Dimensions
	float _width     = 1.0f;
	float _depth     = 1.0f;
	int   _segmentsX = 32;
	int   _segmentsZ = 32;

	// Generation Mode
	int _generatorMode = static_cast<int>(GeneratorMode::FlatPlane);

	// Displacement Map (PNG / TGA / DDS) Parameters
	std::string _displacementMapPath = "";
	const TEXTURE* _displacementPreviewTex = nullptr;
	std::vector<float> _dispPixels; // Normalized [0..1] luminance cache on CPU
	int   _dispWidth  = 0;
	int   _dispHeight = 0;

	float _displacementScale    = 0.5f;
	float _displacementMidpoint = 0.0f; // 0.0 = black is base, 0.5 = mid-gray is base
	bool  _invertDisplacement   = false;
	int   _dispSmoothRadius     = 1;    // 0 = bilinear only, 1..4 = box/gaussian smoothing
	XMFLOAT2 _dispTiling        = { 1.0f, 1.0f };
	XMFLOAT2 _dispOffset        = { 0.0f, 0.0f };

	// Procedural Noise Parameters
	int   _noiseType        = static_cast<int>(NoiseType::PerlinFBM);
	float _noiseAmplitude   = 0.35f;
	float _noiseFrequency   = 3.0f;
	int   _noiseOctaves     = 4;
	float _noisePersistence = 0.5f;
	float _noiseLacunarity  = 2.0f;
	int   _noiseSeed        = 1337;
	float _terraceSteps     = 6.0f;
	bool  _useIslandFalloff = false;
	float _falloffStrength  = 1.5f;

	// CPU Mesh Data (Shared with ColliderMesh)
	std::vector<Vertex>       _vertices;
	std::vector<unsigned int> _indices;
	uint32_t                  _meshRevision = 0;

	// GPU Buffers
	std::unique_ptr<VERTEX_BUFFER> _vertexBuffer;
	std::unique_ptr<INDEX_BUFFER>  _indexBuffer;
	unsigned int _indexCount = 0;

	// Internal Helpers
	bool LoadDisplacementMapCPU(const std::string& path);
	float SampleDisplacementBilinear(float u, float v) const;
	float SampleDisplacementFiltered(float u, float v) const;
	float EvaluateNoiseHeight(float u, float v) const;
	void RecalculateNormals();
	void UploadBuffersToGPU();

	// Virtual extension hooks for derived Procedural Field classes
	virtual float EvaluateHeightAt(float normU, float normV, float localX, float localZ) const;
	virtual void OnPostGenerateVertices(std::vector<Vertex>& vertices, int vertsX, int vertsZ) {}

public:
	ProceduralMesh() = default;
	virtual ~ProceduralMesh() = default;

	void Awake() override;
	void OnDestroy() override;
	void Update() override;
	void Render() const override;
	void ShowUI() override;

	std::string GetComponentName() override { return "ProceduralMesh"; }

	// Core Rebuild API
	void RebuildMesh();
	bool SetDisplacementMapPath(const std::string& path);
	const std::string& GetDisplacementMapPath() const { return _displacementMapPath; }

	// Accessors for ColliderMesh & Gameplay
	const std::vector<Vertex>& GetVertices() const { return _vertices; }
	const std::vector<unsigned int>& GetIndices() const { return _indices; }
	uint32_t GetMeshRevision() const { return _meshRevision; }
	int GetSegmentsX() const { return _segmentsX; }
	int GetSegmentsZ() const { return _segmentsZ; }
	float GetWidth() const { return _width; }
	float GetDepth() const { return _depth; }
	int GetGeneratorMode() const { return _generatorMode; }
	void SetGeneratorMode(int mode) { _generatorMode = mode; RebuildMesh(); }

	// Samples the interpolated mesh height in local space at (localX, localZ)
	float SampleHeightAtLocalXZ(float localX, float localZ) const;

	template <class Archive>
	void serialize(Archive& ar) {
		ar(cereal::base_class<Component>(this));
		try { ar(cereal::make_nvp("Width", _width)); } catch (...) {}
		try { ar(cereal::make_nvp("Depth", _depth)); } catch (...) {}
		try { ar(cereal::make_nvp("SegmentsX", _segmentsX)); } catch (...) {}
		try { ar(cereal::make_nvp("SegmentsZ", _segmentsZ)); } catch (...) {}
		try { ar(cereal::make_nvp("GeneratorMode", _generatorMode)); } catch (...) {}
		try { ar(cereal::make_nvp("DisplacementMapPath", _displacementMapPath)); } catch (...) {}
		try { ar(cereal::make_nvp("DisplacementScale", _displacementScale)); } catch (...) {}
		try { ar(cereal::make_nvp("DisplacementMidpoint", _displacementMidpoint)); } catch (...) {}
		try { ar(cereal::make_nvp("InvertDisplacement", _invertDisplacement)); } catch (...) {}
		try { ar(cereal::make_nvp("DispSmoothRadius", _dispSmoothRadius)); } catch (...) {}
		try {
			ar(cereal::make_nvp("DispTilingX", _dispTiling.x));
			ar(cereal::make_nvp("DispTilingY", _dispTiling.y));
			ar(cereal::make_nvp("DispOffsetX", _dispOffset.x));
			ar(cereal::make_nvp("DispOffsetY", _dispOffset.y));
		} catch (...) {}
		try { ar(cereal::make_nvp("NoiseType", _noiseType)); } catch (...) {}
		try { ar(cereal::make_nvp("NoiseAmplitude", _noiseAmplitude)); } catch (...) {}
		try { ar(cereal::make_nvp("NoiseFrequency", _noiseFrequency)); } catch (...) {}
		try { ar(cereal::make_nvp("NoiseOctaves", _noiseOctaves)); } catch (...) {}
		try { ar(cereal::make_nvp("NoisePersistence", _noisePersistence)); } catch (...) {}
		try { ar(cereal::make_nvp("NoiseLacunarity", _noiseLacunarity)); } catch (...) {}
		try { ar(cereal::make_nvp("NoiseSeed", _noiseSeed)); } catch (...) {}
		try { ar(cereal::make_nvp("TerraceSteps", _terraceSteps)); } catch (...) {}
		try { ar(cereal::make_nvp("UseIslandFalloff", _useIslandFalloff)); } catch (...) {}
		try { ar(cereal::make_nvp("FalloffStrength", _falloffStrength)); } catch (...) {}
	}
};

#endif // !_PROCEDURAL_MESH_H_
