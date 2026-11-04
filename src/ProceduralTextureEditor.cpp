//=====================================================================================
// ProceduralTextureEditor.cpp
// Author:Kaito Aoki
// Date:2026/10/26
//=====================================================================================

#include "ProceduralTextureEditor.h"
#include "Texture.h"
#include "Manager.h"
#include "Scene.h"
#include "GameObject.h"
#include "Shader.h"
#include "imgui_internal.h"

#include <cmath>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <unordered_map>
#include <unordered_set>
#include <commdlg.h>
#include <d3dcompiler.h>
#include <sstream>
#include <iomanip>
#include <cereal/archives/json.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/string.hpp>

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

namespace fs = std::filesystem;

namespace {

// =====================================================================================
// Math & Procedural Noise Helpers
// =====================================================================================
static inline float Clamp01(float v) {
	return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
}

static inline float Frac(float v) {
	return v - std::floor(v);
}

static inline float Lerp(float a, float b, float t) {
	return a + (b - a) * t;
}

static inline float SmoothStep(float t) {
	return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

static inline uint32_t Hash2D(int x, int y, int seed) {
	uint32_t h = static_cast<uint32_t>(x) * 374761393u
	           + static_cast<uint32_t>(y) * 668265263u
	           + static_cast<uint32_t>(seed) * 1442695041u;
	h = (h ^ (h >> 13)) * 1274126177u;
	return h ^ (h >> 16);
}

static inline void Hash2DVec2(int x, int y, int seed, float& outX, float& outY) {
	uint32_t h1 = Hash2D(x, y, seed);
	uint32_t h2 = Hash2D(x ^ 0x5bd1e995, y ^ 0x1b873593, seed + 7919);
	outX = static_cast<float>(h1 & 0xFFFFu) / 65535.0f;
	outY = static_cast<float>(h2 & 0xFFFFu) / 65535.0f;
}

// Tileable 2D gradient noise (periodX, periodY > 0 enables seamless tiling)
static float GradientNoise2D(float x, float y, int periodX, int periodY, int seed) {
	int ix0 = static_cast<int>(std::floor(x));
	int iy0 = static_cast<int>(std::floor(y));
	float fx = x - std::floor(x);
	float fy = y - std::floor(y);

	int ix1 = ix0 + 1;
	int iy1 = iy0 + 1;

	if (periodX > 0) {
		ix0 = ((ix0 % periodX) + periodX) % periodX;
		ix1 = ((ix1 % periodX) + periodX) % periodX;
	}
	if (periodY > 0) {
		iy0 = ((iy0 % periodY) + periodY) % periodY;
		iy1 = ((iy1 % periodY) + periodY) % periodY;
	}

	auto gradDot = [&](int gx, int gy, float dx, float dy) -> float {
		uint32_t h = Hash2D(gx, gy, seed);
		float angle = (static_cast<float>(h & 0xFFFFu) / 65535.0f) * 6.283185307f;
		return std::cos(angle) * dx + std::sin(angle) * dy;
	};

	float n00 = gradDot(ix0, iy0, fx, fy);
	float n10 = gradDot(ix1, iy0, fx - 1.0f, fy);
	float n01 = gradDot(ix0, iy1, fx, fy - 1.0f);
	float n11 = gradDot(ix1, iy1, fx - 1.0f, fy - 1.0f);

	float u = SmoothStep(fx);
	float v = SmoothStep(fy);

	float nx0 = Lerp(n00, n10, u);
	float nx1 = Lerp(n01, n11, u);
	return Lerp(nx0, nx1, v); // roughly [-0.7, 0.7]
}

static float EvaluateFBM(float u, float v, float scaleX, float scaleY, int octaves, float persistence, float lacunarity, int seed, bool ridged) {
	float total = 0.0f;
	float amplitude = 1.0f;
	float maxAmp = 0.0f;
	float freqX = std::max(1.0f, scaleX);
	float freqY = std::max(1.0f, scaleY);

	for (int i = 0; i < octaves; i++) {
		int pX = static_cast<int>( std::round(freqX) );
		int pY = static_cast<int>( std::round(freqY) );
		float n = GradientNoise2D(u * freqX, v * freqY, pX, pY, seed + i * 101);
		if (ridged) {
			n = 1.0f - std::fabs(n * 1.414f);
			n = Clamp01(n);
			n = n * n;
		} else {
			n = Clamp01(n * 0.7071f + 0.5f);
		}
		total += n * amplitude;
		maxAmp += amplitude;
		amplitude *= persistence;
		freqX *= lacunarity;
		freqY *= lacunarity;
	}
	return maxAmp > 0.0f ? Clamp01(total / maxAmp) : 0.0f;
}

static float EvaluateVoronoi(float u, float v, float scale, float jitter, int mode, int seed) {
	int period = std::max(1, static_cast<int>(std::round(scale)));
	float x = u * static_cast<float>(period);
	float y = v * static_cast<float>(period);

	int ix = static_cast<int>(std::floor(x));
	int iy = static_cast<int>(std::floor(y));
	float fx = x - std::floor(x);
	float fy = y - std::floor(y);

	float minDist1 = 100.0f;
	float minDist2 = 100.0f;
	uint32_t closestHash = 0;

	for (int dy = -1; dy <= 1; dy++) {
		for (int dx = -1; dx <= 1; dx++) {
			int cx = ((ix + dx) % period + period) % period;
			int cy = ((iy + dy) % period + period) % period;
			float rx, ry;
			Hash2DVec2(cx, cy, seed, rx, ry);
			float px = static_cast<float>(dx) + 0.5f + (rx - 0.5f) * jitter - fx;
			float py = static_cast<float>(dy) + 0.5f + (ry - 0.5f) * jitter - fy;
			float d = std::sqrt(px * px + py * py);
			if (d < minDist1) {
				minDist2 = minDist1;
				minDist1 = d;
				closestHash = Hash2D(cx, cy, seed + 31);
			} else if (d < minDist2) {
				minDist2 = d;
			}
		}
	}

	if (mode == 0) {
		// F1 distance
		return Clamp01(minDist1);
	} else if (mode == 1) {
		// F2 - F1 edge / cracks
		return Clamp01((minDist2 - minDist1) * 1.5f);
	} else {
		// Cell random value
		return static_cast<float>(closestHash & 0xFFFFu) / 65535.0f;
	}
}

static inline uint32_t Hash3D(int x, int y, int z, int seed) {
	uint32_t h = static_cast<uint32_t>(x) * 374761393u
	           + static_cast<uint32_t>(y) * 668265263u
	           + static_cast<uint32_t>(z) * 2147483647u
	           + static_cast<uint32_t>(seed) * 1442695041u;
	h = (h ^ (h >> 13)) * 1274126177u;
	return h ^ (h >> 16);
}

static inline void Hash3DVec3(int x, int y, int z, int seed, float& outX, float& outY, float& outZ) {
	uint32_t h1 = Hash3D(x, y, z, seed);
	uint32_t h2 = Hash3D(x ^ 0x5bd1e995, y ^ 0x1b873593, z ^ 0xcc9e2d51, seed + 7919);
	uint32_t h3 = Hash3D(x ^ 0x85ebca6b, y ^ 0xc2b2ae35, z ^ 0x27d4eb2f, seed + 16127);
	outX = static_cast<float>(h1 & 0xFFFFu) / 65535.0f;
	outY = static_cast<float>(h2 & 0xFFFFu) / 65535.0f;
	outZ = static_cast<float>(h3 & 0xFFFFu) / 65535.0f;
}

static float GradientNoise3D(float x, float y, float z, int seed) {
	int ix0 = static_cast<int>(std::floor(x));
	int iy0 = static_cast<int>(std::floor(y));
	int iz0 = static_cast<int>(std::floor(z));
	float fx = x - std::floor(x);
	float fy = y - std::floor(y);
	float fz = z - std::floor(z);

	int ix1 = ix0 + 1;
	int iy1 = iy0 + 1;
	int iz1 = iz0 + 1;

	auto gradDot3D = [&](int gx, int gy, int gz, float dx, float dy, float dz) -> float {
		switch (Hash3D(gx, gy, gz, seed) & 15u) {
		case 0:  return  dx + dy;
		case 1:  return -dx + dy;
		case 2:  return  dx - dy;
		case 3:  return -dx - dy;
		case 4:  return  dx + dz;
		case 5:  return -dx + dz;
		case 6:  return  dx - dz;
		case 7:  return -dx - dz;
		case 8:  return  dy + dz;
		case 9:  return -dy + dz;
		case 10: return  dy - dz;
		case 11: return -dy - dz;
		case 12: return  dx + dy;
		case 13: return -dy + dz;
		case 14: return -dx + dy;
		default: return -dy - dz;
		}
	};

	float u = SmoothStep(fx);
	float v = SmoothStep(fy);
	float w = SmoothStep(fz);

	float n000 = gradDot3D(ix0, iy0, iz0, fx,        fy,        fz);
	float n100 = gradDot3D(ix1, iy0, iz0, fx - 1.0f, fy,        fz);
	float n010 = gradDot3D(ix0, iy1, iz0, fx,        fy - 1.0f, fz);
	float n110 = gradDot3D(ix1, iy1, iz0, fx - 1.0f, fy - 1.0f, fz);
	float n001 = gradDot3D(ix0, iy0, iz1, fx,        fy,        fz - 1.0f);
	float n101 = gradDot3D(ix1, iy0, iz1, fx - 1.0f, fy,        fz - 1.0f);
	float n011 = gradDot3D(ix0, iy1, iz1, fx,        fy - 1.0f, fz - 1.0f);
	float n111 = gradDot3D(ix1, iy1, iz1, fx - 1.0f, fy - 1.0f, fz - 1.0f);

	float nx00 = Lerp(n000, n100, u);
	float nx10 = Lerp(n010, n110, u);
	float nx01 = Lerp(n001, n101, u);
	float nx11 = Lerp(n011, n111, u);

	float nxy0 = Lerp(nx00, nx10, v);
	float nxy1 = Lerp(nx01, nx11, v);
	return Lerp(nxy0, nxy1, w);
}

static float EvaluateFBM3D(float px, float py, float pz, float scaleX, float scaleY, int octaves, float persistence, float lacunarity, int seed, bool ridged) {
	float total = 0.0f;
	float amplitude = 1.0f;
	float maxAmp = 0.0f;
	float freqXZ = std::max(0.5f, scaleX * 0.5f);
	float freqY  = std::max(0.5f, scaleY * 0.5f);

	for (int i = 0; i < octaves; i++) {
		float n = GradientNoise3D((px + 3.7f) * freqXZ, (py + 3.7f) * freqY, (pz + 3.7f) * freqXZ, seed + i * 101);
		if (ridged) {
			n = 1.0f - std::fabs(n * 1.25f);
			n = Clamp01(n);
			n = n * n;
		} else {
			n = Clamp01(n * 0.65f + 0.5f);
		}
		total += n * amplitude;
		maxAmp += amplitude;
		amplitude *= persistence;
		freqXZ *= lacunarity;
		freqY  *= lacunarity;
	}
	return maxAmp > 0.0f ? Clamp01(total / maxAmp) : 0.0f;
}

static float EvaluateVoronoi3D(float px, float py, float pz, float scale, float jitter, int mode, int seed) {
	float freq = std::max(0.5f, scale * 0.5f);
	float x = (px + 5.0f) * freq;
	float y = (py + 5.0f) * freq;
	float z = (pz + 5.0f) * freq;

	int ix = static_cast<int>(std::floor(x));
	int iy = static_cast<int>(std::floor(y));
	int iz = static_cast<int>(std::floor(z));
	float fx = x - std::floor(x);
	float fy = y - std::floor(y);
	float fz = z - std::floor(z);

	float minDist1 = 100.0f;
	float minDist2 = 100.0f;
	uint32_t closestHash = 0;

	for (int dz = -1; dz <= 1; dz++) {
		for (int dy = -1; dy <= 1; dy++) {
			for (int dx = -1; dx <= 1; dx++) {
				int cx = ix + dx;
				int cy = iy + dy;
				int cz = iz + dz;
				float rx, ry, rz;
				Hash3DVec3(cx, cy, cz, seed, rx, ry, rz);
				float vx = static_cast<float>(dx) + 0.5f + (rx - 0.5f) * jitter - fx;
				float vy = static_cast<float>(dy) + 0.5f + (ry - 0.5f) * jitter - fy;
				float vz = static_cast<float>(dz) + 0.5f + (rz - 0.5f) * jitter - fz;
				float d = std::sqrt(vx * vx + vy * vy + vz * vz);
				if (d < minDist1) {
					minDist2 = minDist1;
					minDist1 = d;
					closestHash = Hash3D(cx, cy, cz, seed + 31);
				} else if (d < minDist2) {
					minDist2 = d;
				}
			}
		}
	}

	if (mode == 0) {
		return Clamp01(minDist1);
	} else if (mode == 1) {
		return Clamp01((minDist2 - minDist1) * 1.5f);
	} else {
		return static_cast<float>(closestHash & 0xFFFFu) / 65535.0f;
	}
}

static inline void MapUVTo3D(float u, float v, int mappingSpace, float& outX, float& outY, float& outZ) {
	const float PI = 3.14159265359f;
	float phi = u * 2.0f * PI;
	float theta = Clamp01(v) * PI;
	float sinT = std::sin(theta);
	float dx = sinT * std::cos(phi);
	float dy = std::cos(theta);
	float dz = sinT * std::sin(phi);

	if (mappingSpace == 2) {
		// 3D Box / Cube-Sphere projection (projects unit sphere onto unit cube [-1, 1]^3)
		float maxAxis = std::max(std::max(std::fabs(dx), std::fabs(dy)), std::fabs(dz));
		if (maxAxis > 0.0001f) {
			outX = dx / maxAxis;
			outY = dy / maxAxis;
			outZ = dz / maxAxis;
		} else {
			outX = dx; outY = dy; outZ = dz;
		}
	} else {
		// 3D Spherical (Unit sphere)
		outX = dx;
		outY = dy;
		outZ = dz;
	}
}

static inline void Dir3DToSphericalUV(float dx, float dy, float dz, float& outU, float& outV) {
	const float PI = 3.14159265359f;
	float len = std::sqrt(dx * dx + dy * dy + dz * dz);
	if (len > 0.0001f) {
		dx /= len; dy /= len; dz /= len;
	} else {
		dx = 0.0f; dy = 1.0f; dz = 0.0f;
	}
	dy = std::clamp(dy, -1.0f, 1.0f);
	float theta = std::acos(dy);
	float phi = std::atan2(dz, dx);
	if (phi < 0.0f) phi += 2.0f * PI;
	outU = Frac(phi / (2.0f * PI));
	outV = Clamp01(theta / PI);
}

static inline void SampleBilinearWrap(const std::vector<float>& buf, int w, int h, float u, float v, float outRGBA[4]) {
	if (buf.empty() || w <= 0 || h <= 0) {
		outRGBA[0] = outRGBA[1] = outRGBA[2] = 0.0f;
		outRGBA[3] = 1.0f;
		return;
	}
	u = Frac(u);
	v = Frac(v);
	float px = u * w - 0.5f;
	float py = v * h - 0.5f;
	int x0 = static_cast<int>(std::floor(px));
	int y0 = static_cast<int>(std::floor(py));
	float fx = px - std::floor(px);
	float fy = py - std::floor(py);

	int x0w = ((x0 % w) + w) % w;
	int x1w = (((x0 + 1) % w) + w) % w;
	int y0w = ((y0 % h) + h) % h;
	int y1w = (((y0 + 1) % h) + h) % h;

	const float* c00 = &buf[(y0w * w + x0w) * 4];
	const float* c10 = &buf[(y0w * w + x1w) * 4];
	const float* c01 = &buf[(y1w * w + x0w) * 4];
	const float* c11 = &buf[(y1w * w + x1w) * 4];

	for (int c = 0; c < 4; c++) {
		float top = Lerp(c00[c], c10[c], fx);
		float bot = Lerp(c01[c], c11[c], fx);
		outRGBA[c] = Lerp(top, bot, fy);
	}
}

static inline float Luminance(const float rgba[4]) {
	return Clamp01(rgba[0] * 0.299f + rgba[1] * 0.587f + rgba[2] * 0.114f);
}

// =====================================================================================
// Node Pin Metadata
// =====================================================================================
struct NodeMeta {
	const char* title;
	ImU32 headerColor;
	std::vector<const char*> inputPins;
	std::vector<const char*> outputPins;
};

static NodeMeta GetNodeMeta(ProcNodeType type) {
	switch (type) {
	case ProcNodeType::PerlinNoise:
		return { "Perlin / FBM Noise", IM_COL32(45, 110, 75, 255), { "UV (Opt)" }, { "Out" } };
	case ProcNodeType::Voronoi:
		return { "Voronoi / Cellular", IM_COL32(45, 110, 75, 255), { "UV (Opt)" }, { "Out" } };
	case ProcNodeType::Checker:
		return { "Checker / Grid", IM_COL32(45, 110, 75, 255), { "UV (Opt)" }, { "Out" } };
	case ProcNodeType::Brick:
		return { "Brick Pattern", IM_COL32(45, 110, 75, 255), { "UV (Opt)" }, { "Out" } };
	case ProcNodeType::Gradient:
		return { "Gradient", IM_COL32(45, 110, 75, 255), { "UV (Opt)" }, { "Out" } };
	case ProcNodeType::ConstantColor:
		return { "Constant Color", IM_COL32(45, 110, 75, 255), {}, { "Out" } };
	case ProcNodeType::Blend:
		return { "Blend", IM_COL32(60, 95, 155, 255), { "Base", "Blend", "Mask (Opt)" }, { "Out" } };
	case ProcNodeType::ColorRamp:
		return { "Color Ramp", IM_COL32(60, 95, 155, 255), { "Factor" }, { "Out" } };
	case ProcNodeType::UVWarp:
		return { "UV Warp", IM_COL32(130, 85, 150, 255), { "Source (Opt)", "Warp Map" }, { "Out / UV" } };
	case ProcNodeType::Levels:
		return { "Levels / Math", IM_COL32(60, 95, 155, 255), { "Input" }, { "Out" } };
	case ProcNodeType::NormalMap:
		return { "Height to Normal", IM_COL32(105, 85, 165, 255), { "Height" }, { "Normal" } };
	case ProcNodeType::Output:
		return { "Texture Output", IM_COL32(165, 65, 55, 255), { "Albedo", "Normal", "Roughness", "Metallic", "AO" }, {} };
	case ProcNodeType::CustomHLSL:
		return { "Custom HLSL", IM_COL32(175, 115, 35, 255), { "UV (Opt)", "In A (Opt)", "In B (Opt)" }, { "Out" } };
	default:
		return { "Unknown", IM_COL32(80, 80, 80, 255), {}, { "Out" } };
	}
}

// =====================================================================================
// Editor State
// =====================================================================================
static bool s_initialized = false;
static std::vector<ProcNode> s_nodes;
static std::vector<ProcNodeLink> s_links;
static int s_nextNodeId = 1;
static int s_nextLinkId = 1;
static int s_selectedNodeId = -1;
static bool s_graphDirty = true;

// Canvas interaction state
static ImVec2 s_canvasScroll = ImVec2(0.0f, 0.0f);
static int s_draggingPinNodeId = -1;
static int s_draggingPinIdx = -1;
static bool s_draggingPinIsOutput = true;

// Preview & Export settings
static int s_previewResolution = 128;   // Live preview resolution
static int s_exportResIdx = 1;          // 0:256, 1:512, 2:1024, 3:2048
static int s_previewChannel = 0;        // 0:Albedo, 1:Normal, 2:ARM, 3:Selected Node
static bool s_tilePreview2x2 = false;
static char s_exportNameBuf[128] = "proc_brick";
static bool s_exportNormal = true;
static bool s_exportARM = true;
static std::string s_statusMessage = "";
static float s_statusTimer = 0.0f;

// Output channel buffers for live preview / export
static std::vector<float> s_outAlbedoRGBA;
static std::vector<float> s_outNormalRGBA;
static std::vector<float> s_outArmRGBA;
static const TEXTURE* s_outAlbedoTex = nullptr;
static const TEXTURE* s_outNormalTex = nullptr;
static const TEXTURE* s_outArmTex = nullptr;

// 3D Mapping Space & 3D Interactive Preview state
static int s_mappingSpace = 0;          // 0: 2D Planar (Tileable), 1: 3D Spherical (SkyDome / Sphere), 2: 3D Triplanar / Box (Cube)
static int s_previewShapeMode = 0;      // 0: 2D Flat, 1: 3D Sphere, 2: 3D Cube, 3: 3D SkyDome (Inside)
static bool s_previewPBRShading = true; // Enable PBR lighting (Normal + Roughness + Metallic + AO) in 3D preview
static float s_previewYaw = 0.45f;
static float s_previewPitch = 0.25f;
static float s_lightYaw = -0.65f;
static float s_lightPitch = 0.55f;
static bool s_preview3DDirty = true;
static const TEXTURE* s_preview3DTex = nullptr;

// Hybrid Editor Mode (0: Node Graph, 1: HLSL Code Editor) & D3D12 Compute Pipeline State
static int s_editorTabMode = 0;         // 0: Node Graph, 1: HLSL Code (DirectX 12 Compute)
static char s_hlslFileNameBuf[128] = "proc_custom_swirl";
static std::string s_hlslCodeText = "";
static std::string s_hlslCompileLog = "";
static bool s_hlslLastCompileSuccess = true;
static ComPtr<ID3D12RootSignature> s_computeRootSig;
static std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> s_computePsoCache;

static ProcNode* FindNode(int id) {
	for (auto& n : s_nodes) {
		if (n.id == id) return &n;
	}
	return nullptr;
}

static ProcNode* FindOutputNode() {
	for (auto& n : s_nodes) {
		if (n.type == ProcNodeType::Output) return &n;
	}
	return nullptr;
}

static ProcNode* GetInputSourceNode(int toNodeId, int toPinIdx) {
	for (const auto& link : s_links) {
		if (link.toNodeId == toNodeId && link.toPinIdx == toPinIdx) {
			return FindNode(link.fromNodeId);
		}
	}
	return nullptr;
}

static ProcNode& AddNode(ProcNodeType type, ImVec2 pos) {
	ProcNode node;
	node.id = s_nextNodeId++;
	node.type = type;
	node.title = GetNodeMeta(type).title;
	node.pos = pos;

	// Sensible defaults per node type
	switch (type) {
	case ProcNodeType::PerlinNoise:
		node.scaleX = 4.0f;
		node.scaleY = 4.0f;
		node.octaves = 4;
		node.persistence = 0.5f;
		node.lacunarity = 2.0f;
		node.seed = 1337;
		node.flagA = false; // Ridged
		break;
	case ProcNodeType::Voronoi:
		node.scaleX = 5.0f;
		node.paramA = 0.85f; // Jitter
		node.mode = 0;       // 0:F1, 1:Edge, 2:Cell
		node.seed = 42;
		node.flagA = false;  // Invert
		break;
	case ProcNodeType::Checker:
		node.scaleX = 4.0f;
		node.scaleY = 4.0f;
		node.colorA[0] = 0.9f; node.colorA[1] = 0.9f; node.colorA[2] = 0.9f; node.colorA[3] = 1.0f;
		node.colorB[0] = 0.15f; node.colorB[1] = 0.15f; node.colorB[2] = 0.18f; node.colorB[3] = 1.0f;
		node.flagA = false;  // Grid mode
		node.paramA = 0.08f; // Grid line width
		break;
	case ProcNodeType::Brick:
		node.scaleX = 4.0f;  // Columns
		node.scaleY = 8.0f;  // Rows
		node.paramA = 0.08f; // Mortar size
		node.paramB = 0.5f;  // Row offset
		node.paramC = 0.15f; // Brick color variation
		node.colorA[0] = 0.65f; node.colorA[1] = 0.24f; node.colorA[2] = 0.16f; node.colorA[3] = 1.0f; // Brick
		node.colorB[0] = 0.78f; node.colorB[1] = 0.76f; node.colorB[2] = 0.72f; node.colorB[3] = 1.0f; // Mortar
		break;
	case ProcNodeType::Gradient:
		node.mode = 0;       // 0:Linear, 1:Radial
		node.paramA = 0.0f;  // Angle degrees
		node.scaleX = 1.0f;  // Repeat
		node.colorA[0] = 0.1f; node.colorA[1] = 0.1f; node.colorA[2] = 0.1f; node.colorA[3] = 1.0f;
		node.colorB[0] = 0.95f; node.colorB[1] = 0.95f; node.colorB[2] = 0.95f; node.colorB[3] = 1.0f;
		break;
	case ProcNodeType::ConstantColor:
		node.colorA[0] = 0.8f; node.colorA[1] = 0.5f; node.colorA[2] = 0.2f; node.colorA[3] = 1.0f;
		break;
	case ProcNodeType::Blend:
		node.mode = 0;       // 0:Mix, 1:Multiply, 2:Add, 3:Subtract, 4:Screen, 5:Overlay
		node.paramA = 0.5f;  // Opacity
		break;
	case ProcNodeType::ColorRamp:
		node.paramD = 0.0f;  // Stop 0 pos
		node.paramB = 0.5f;  // Stop 1 pos
		node.paramC = 1.0f;  // Stop 2 pos
		node.flagA = true;   // Use middle stop
		node.colorA[0] = 0.10f; node.colorA[1] = 0.08f; node.colorA[2] = 0.07f; node.colorA[3] = 1.0f;
		node.colorB[0] = 0.55f; node.colorB[1] = 0.35f; node.colorB[2] = 0.20f; node.colorB[3] = 1.0f;
		node.colorC[0] = 0.92f; node.colorC[1] = 0.85f; node.colorC[2] = 0.72f; node.colorC[3] = 1.0f;
		break;
	case ProcNodeType::UVWarp:
		node.paramA = 0.12f; // Warp strength
		break;
	case ProcNodeType::Levels:
		node.paramA = 0.0f;  // Brightness
		node.paramB = 1.0f;  // Contrast
		node.paramC = 1.0f;  // Gamma
		node.flagA = false;  // Invert
		break;
	case ProcNodeType::NormalMap:
		node.paramA = 4.0f;  // Strength
		node.flagA = false;  // Invert Y
		break;
	case ProcNodeType::Output:
		node.paramA = 0.6f;  // Default Roughness
		node.paramB = 0.0f;  // Default Metallic
		node.paramC = 1.0f;  // Default AO
		break;
	case ProcNodeType::CustomHLSL:
		node.customHlslFile = "shader/proc_custom_swirl.hlsl";
		node.scaleX = 4.0f;
		node.scaleY = 4.0f;
		node.paramA = 0.5f;
		node.paramB = 0.5f;
		node.paramC = 1.0f;
		node.paramD = 0.0f;
		node.colorA[0] = 0.95f; node.colorA[1] = 0.55f; node.colorA[2] = 0.15f; node.colorA[3] = 1.0f;
		node.colorB[0] = 0.10f; node.colorB[1] = 0.25f; node.colorB[2] = 0.55f; node.colorB[3] = 1.0f;
		break;
	}

	s_nodes.push_back(node);
	s_graphDirty = true;
	return s_nodes.back();
}

static void AddLink(int fromNodeId, int fromPinIdx, int toNodeId, int toPinIdx) {
	if (fromNodeId == toNodeId) return;
	// Remove any existing link into the same input pin
	s_links.erase(
		std::remove_if(s_links.begin(), s_links.end(), [&](const ProcNodeLink& l) {
			return l.toNodeId == toNodeId && l.toPinIdx == toPinIdx;
		}),
		s_links.end()
	);
	ProcNodeLink link;
	link.id = s_nextLinkId++;
	link.fromNodeId = fromNodeId;
	link.fromPinIdx = fromPinIdx;
	link.toNodeId = toNodeId;
	link.toPinIdx = toPinIdx;
	s_links.push_back(link);
	s_graphDirty = true;
}

// =====================================================================================
// Built-in Sample Presets
// =====================================================================================
static void LoadPresetOutputOnly() {
	s_nodes.clear();
	s_links.clear();
	s_nextNodeId = 1;
	s_nextLinkId = 1;

	int outId = AddNode(ProcNodeType::Output, ImVec2(480.0f, 160.0f)).id;

	strcpy_s(s_exportNameBuf, "proc_texture");
	s_mappingSpace = 0;
	s_selectedNodeId = outId;
	s_graphDirty = true;
}

static void LoadPresetBrickWall() {
	s_nodes.clear();
	s_links.clear();
	s_nextNodeId = 1;
	s_nextLinkId = 1;

	int brickId = AddNode(ProcNodeType::Brick, ImVec2(40.0f, 40.0f)).id;
	int noiseId = AddNode(ProcNodeType::PerlinNoise, ImVec2(40.0f, 340.0f)).id;
	FindNode(noiseId)->scaleX = 8.0f;
	FindNode(noiseId)->scaleY = 8.0f;
	FindNode(noiseId)->octaves = 5;

	int blendId = AddNode(ProcNodeType::Blend, ImVec2(320.0f, 60.0f)).id;
	FindNode(blendId)->mode = 1; // Multiply
	FindNode(blendId)->paramA = 0.35f;

	int heightBlendId = AddNode(ProcNodeType::Blend, ImVec2(320.0f, 300.0f)).id;
	FindNode(heightBlendId)->mode = 1; // Multiply
	FindNode(heightBlendId)->paramA = 0.45f;

	int normalId = AddNode(ProcNodeType::NormalMap, ImVec2(580.0f, 260.0f)).id;
	FindNode(normalId)->paramA = 5.5f;

	int roughLevelsId = AddNode(ProcNodeType::Levels, ImVec2(580.0f, 460.0f)).id;
	FindNode(roughLevelsId)->paramA = 0.25f;
	FindNode(roughLevelsId)->paramB = 0.8f;
	FindNode(roughLevelsId)->flagA = true; // Invert so mortar is rougher

	int outId = AddNode(ProcNodeType::Output, ImVec2(840.0f, 160.0f)).id;

	AddLink(brickId, 0, blendId, 0);
	AddLink(noiseId, 0, blendId, 1);

	AddLink(brickId, 0, heightBlendId, 0);
	AddLink(noiseId, 0, heightBlendId, 1);

	AddLink(heightBlendId, 0, normalId, 0);
	AddLink(heightBlendId, 0, roughLevelsId, 0);

	AddLink(blendId, 0, outId, 0);       // Albedo
	AddLink(normalId, 0, outId, 1);      // Normal
	AddLink(roughLevelsId, 0, outId, 2); // Roughness

	strcpy_s(s_exportNameBuf, "proc_brick");
	s_mappingSpace = 0;
	s_selectedNodeId = outId;
	s_graphDirty = true;
}

static void LoadPresetMarble() {
	s_nodes.clear();
	s_links.clear();
	s_nextNodeId = 1;
	s_nextLinkId = 1;

	int noiseId = AddNode(ProcNodeType::PerlinNoise, ImVec2(40.0f, 60.0f)).id;
	FindNode(noiseId)->scaleX = 3.0f;
	FindNode(noiseId)->scaleY = 3.0f;
	FindNode(noiseId)->octaves = 6;
	FindNode(noiseId)->flagA = true; // Ridged veins

	int gradId = AddNode(ProcNodeType::Gradient, ImVec2(40.0f, 350.0f)).id;
	FindNode(gradId)->scaleX = 4.0f;
	FindNode(gradId)->paramA = 35.0f;

	int warpId = AddNode(ProcNodeType::UVWarp, ImVec2(320.0f, 180.0f)).id;
	FindNode(warpId)->paramA = 0.35f;

	int rampId = AddNode(ProcNodeType::ColorRamp, ImVec2(580.0f, 120.0f)).id;
	{
		ProcNode* r = FindNode(rampId);
		r->paramD = 0.0f;
		r->paramB = 0.35f;
		r->paramC = 0.85f;
		r->colorA[0] = 0.16f; r->colorA[1] = 0.22f; r->colorA[2] = 0.26f;
		r->colorB[0] = 0.72f; r->colorB[1] = 0.76f; r->colorB[2] = 0.78f;
		r->colorC[0] = 0.96f; r->colorC[1] = 0.95f; r->colorC[2] = 0.93f;
	}

	int normalId = AddNode(ProcNodeType::NormalMap, ImVec2(580.0f, 400.0f)).id;
	FindNode(normalId)->paramA = 1.8f;

	int outId = AddNode(ProcNodeType::Output, ImVec2(850.0f, 180.0f)).id;
	FindNode(outId)->paramA = 0.2f; // Polished low roughness

	AddLink(gradId, 0, warpId, 0);
	AddLink(noiseId, 0, warpId, 1);
	AddLink(warpId, 0, rampId, 0);
	AddLink(warpId, 0, normalId, 0);
	AddLink(rampId, 0, outId, 0);
	AddLink(normalId, 0, outId, 1);

	strcpy_s(s_exportNameBuf, "proc_marble");
	s_mappingSpace = 0;
	s_selectedNodeId = outId;
	s_graphDirty = true;
}

static void LoadPresetCrackedLava() {
	s_nodes.clear();
	s_links.clear();
	s_nextNodeId = 1;
	s_nextLinkId = 1;

	int voronoiId = AddNode(ProcNodeType::Voronoi, ImVec2(40.0f, 60.0f)).id;
	FindNode(voronoiId)->scaleX = 6.0f;
	FindNode(voronoiId)->mode = 1; // F2-F1 Cracks
	FindNode(voronoiId)->paramA = 0.9f;

	int noiseId = AddNode(ProcNodeType::PerlinNoise, ImVec2(40.0f, 340.0f)).id;
	FindNode(noiseId)->scaleX = 6.0f;
	FindNode(noiseId)->scaleY = 6.0f;

	int blendId = AddNode(ProcNodeType::Blend, ImVec2(320.0f, 140.0f)).id;
	FindNode(blendId)->mode = 1; // Multiply
	FindNode(blendId)->paramA = 0.45f;

	int rampId = AddNode(ProcNodeType::ColorRamp, ImVec2(580.0f, 80.0f)).id;
	{
		ProcNode* r = FindNode(rampId);
		r->paramD = 0.02f;
		r->paramB = 0.16f;
		r->paramC = 0.45f;
		r->colorA[0] = 1.00f; r->colorA[1] = 0.45f; r->colorA[2] = 0.05f; // Glowing crack
		r->colorB[0] = 0.55f; r->colorB[1] = 0.12f; r->colorB[2] = 0.05f; // Warm edge
		r->colorC[0] = 0.14f; r->colorC[1] = 0.13f; r->colorC[2] = 0.15f; // Dark basalt rock
	}

	int normalId = AddNode(ProcNodeType::NormalMap, ImVec2(580.0f, 380.0f)).id;
	FindNode(normalId)->paramA = 7.0f;

	int outId = AddNode(ProcNodeType::Output, ImVec2(850.0f, 180.0f)).id;
	FindNode(outId)->paramA = 0.75f;

	AddLink(voronoiId, 0, blendId, 0);
	AddLink(noiseId, 0, blendId, 1);
	AddLink(blendId, 0, rampId, 0);
	AddLink(blendId, 0, normalId, 0);
	AddLink(rampId, 0, outId, 0);
	AddLink(normalId, 0, outId, 1);
	AddLink(voronoiId, 0, outId, 4); // AO in cracks

	strcpy_s(s_exportNameBuf, "proc_lava_rock");
	s_mappingSpace = 0;
	s_selectedNodeId = outId;
	s_graphDirty = true;
}

static void LoadPresetRustedMetal() {
	s_nodes.clear();
	s_links.clear();
	s_nextNodeId = 1;
	s_nextLinkId = 1;

	int gridId = AddNode(ProcNodeType::Checker, ImVec2(40.0f, 40.0f)).id;
	FindNode(gridId)->flagA = true; // Grid mode
	FindNode(gridId)->scaleX = 4.0f;
	FindNode(gridId)->scaleY = 4.0f;
	FindNode(gridId)->paramA = 0.06f;
	FindNode(gridId)->colorA[0] = 0.52f; FindNode(gridId)->colorA[1] = 0.55f; FindNode(gridId)->colorA[2] = 0.58f;
	FindNode(gridId)->colorB[0] = 0.20f; FindNode(gridId)->colorB[1] = 0.22f; FindNode(gridId)->colorB[2] = 0.24f;

	int rustNoiseId = AddNode(ProcNodeType::PerlinNoise, ImVec2(40.0f, 340.0f)).id;
	FindNode(rustNoiseId)->scaleX = 5.0f;
	FindNode(rustNoiseId)->scaleY = 5.0f;
	FindNode(rustNoiseId)->octaves = 5;

	int rustRampId = AddNode(ProcNodeType::ColorRamp, ImVec2(320.0f, 340.0f)).id;
	{
		ProcNode* r = FindNode(rustRampId);
		r->paramD = 0.42f;
		r->paramB = 0.58f;
		r->paramC = 0.80f;
		r->colorA[0] = 0.0f;  r->colorA[1] = 0.0f;  r->colorA[2] = 0.0f;
		r->colorB[0] = 0.45f; r->colorB[1] = 0.20f; r->colorB[2] = 0.08f;
		r->colorC[0] = 0.72f; r->colorC[1] = 0.32f; r->colorC[2] = 0.12f;
	}

	int blendId = AddNode(ProcNodeType::Blend, ImVec2(580.0f, 80.0f)).id;
	FindNode(blendId)->mode = 0; // Mix with mask
	FindNode(blendId)->paramA = 0.85f;

	int normalId = AddNode(ProcNodeType::NormalMap, ImVec2(580.0f, 320.0f)).id;
	FindNode(normalId)->paramA = 5.0f;

	int metalLevelsId = AddNode(ProcNodeType::Levels, ImVec2(580.0f, 500.0f)).id;
	FindNode(metalLevelsId)->paramB = 2.2f;
	FindNode(metalLevelsId)->flagA = true; // Invert rust for metallic

	int outId = AddNode(ProcNodeType::Output, ImVec2(850.0f, 180.0f)).id;

	AddLink(gridId, 0, blendId, 0);
	AddLink(rustRampId, 0, blendId, 1);
	AddLink(rustNoiseId, 0, blendId, 2);
	AddLink(rustNoiseId, 0, rustRampId, 0);
	AddLink(blendId, 0, normalId, 0);
	AddLink(rustNoiseId, 0, metalLevelsId, 0);

	AddLink(blendId, 0, outId, 0);       // Albedo
	AddLink(normalId, 0, outId, 1);      // Normal
	AddLink(rustNoiseId, 0, outId, 2);   // Roughness (rust is rough)
	AddLink(metalLevelsId, 0, outId, 3); // Metallic (non-rust is metallic)

	strcpy_s(s_exportNameBuf, "proc_rusted_metal");
	s_mappingSpace = 0;
	s_selectedNodeId = outId;
	s_graphDirty = true;
}

// =====================================================================================
// Node Evaluation Logic
// =====================================================================================
static void LoadPresetSkyDomeClouds() {
	s_nodes.clear();
	s_links.clear();
	s_nextNodeId = 1;
	s_nextLinkId = 1;
	s_mappingSpace = 1;      // 3D Spherical (SkyDome / Sphere)
	s_previewShapeMode = 3;  // 3D SkyDome (Inside)

	int cloudNoiseId = AddNode(ProcNodeType::PerlinNoise, ImVec2(40.0f, 40.0f)).id;
	FindNode(cloudNoiseId)->scaleX = 4.0f;
	FindNode(cloudNoiseId)->scaleY = 6.0f;
	FindNode(cloudNoiseId)->octaves = 6;
	FindNode(cloudNoiseId)->persistence = 0.55f;
	FindNode(cloudNoiseId)->seed = 777;

	int warpNoiseId = AddNode(ProcNodeType::PerlinNoise, ImVec2(40.0f, 340.0f)).id;
	FindNode(warpNoiseId)->scaleX = 3.0f;
	FindNode(warpNoiseId)->scaleY = 3.0f;
	FindNode(warpNoiseId)->octaves = 4;
	FindNode(warpNoiseId)->seed = 1024;

	int warpId = AddNode(ProcNodeType::UVWarp, ImVec2(310.0f, 220.0f)).id;
	FindNode(warpId)->paramA = 0.22f;

	int skyGradId = AddNode(ProcNodeType::Gradient, ImVec2(310.0f, 40.0f)).id;
	FindNode(skyGradId)->mode = 0;
	FindNode(skyGradId)->paramA = 0.0f;
	FindNode(skyGradId)->scaleX = 1.0f;
	FindNode(skyGradId)->colorA[0] = 0.10f; FindNode(skyGradId)->colorA[1] = 0.28f; FindNode(skyGradId)->colorA[2] = 0.68f;
	FindNode(skyGradId)->colorB[0] = 0.58f; FindNode(skyGradId)->colorB[1] = 0.78f; FindNode(skyGradId)->colorB[2] = 0.96f;

	int cloudRampId = AddNode(ProcNodeType::ColorRamp, ImVec2(570.0f, 260.0f)).id;
	{
		ProcNode* r = FindNode(cloudRampId);
		r->paramD = 0.45f;
		r->paramB = 0.62f;
		r->paramC = 0.85f;
		r->colorA[0] = 0.0f;  r->colorA[1] = 0.0f;  r->colorA[2] = 0.0f;
		r->colorB[0] = 0.78f; r->colorB[1] = 0.84f; r->colorB[2] = 0.92f;
		r->colorC[0] = 1.00f; r->colorC[1] = 0.99f; r->colorC[2] = 0.97f;
	}

	int blendId = AddNode(ProcNodeType::Blend, ImVec2(570.0f, 60.0f)).id;
	FindNode(blendId)->mode = 4; // Screen
	FindNode(blendId)->paramA = 0.92f;

	int outId = AddNode(ProcNodeType::Output, ImVec2(850.0f, 140.0f)).id;
	FindNode(outId)->paramA = 1.0f;

	AddLink(warpNoiseId, 0, warpId, 1);
	AddLink(warpId, 0, cloudNoiseId, 0);
	AddLink(cloudNoiseId, 0, cloudRampId, 0);
	AddLink(skyGradId, 0, blendId, 0);
	AddLink(cloudRampId, 0, blendId, 1);
	AddLink(cloudRampId, 0, blendId, 2);
	AddLink(blendId, 0, outId, 0);

	strcpy_s(s_exportNameBuf, "proc_skydome_clouds");
	s_selectedNodeId = outId;
	s_graphDirty = true;
}

static void LoadPresetPlanetSurface() {
	s_nodes.clear();
	s_links.clear();
	s_nextNodeId = 1;
	s_nextLinkId = 1;
	s_mappingSpace = 1;      // 3D Spherical (SkyDome / Sphere)
	s_previewShapeMode = 1;  // 3D Sphere
	s_previewPBRShading = true;

	int contNoiseId = AddNode(ProcNodeType::PerlinNoise, ImVec2(40.0f, 40.0f)).id;
	FindNode(contNoiseId)->scaleX = 4.0f;
	FindNode(contNoiseId)->scaleY = 4.0f;
	FindNode(contNoiseId)->octaves = 6;
	FindNode(contNoiseId)->persistence = 0.55f;
	FindNode(contNoiseId)->seed = 3141;

	int craterId = AddNode(ProcNodeType::Voronoi, ImVec2(40.0f, 340.0f)).id;
	FindNode(craterId)->scaleX = 8.0f;
	FindNode(craterId)->mode = 0; // F1 craters
	FindNode(craterId)->paramA = 0.9f;
	FindNode(craterId)->seed = 901;

	int heightBlendId = AddNode(ProcNodeType::Blend, ImVec2(320.0f, 160.0f)).id;
	FindNode(heightBlendId)->mode = 5; // Overlay
	FindNode(heightBlendId)->paramA = 0.35f;

	int colorRampId = AddNode(ProcNodeType::ColorRamp, ImVec2(580.0f, 60.0f)).id;
	{
		ProcNode* r = FindNode(colorRampId);
		r->paramD = 0.35f;
		r->paramB = 0.50f;
		r->paramC = 0.78f;
		r->colorA[0] = 0.08f; r->colorA[1] = 0.22f; r->colorA[2] = 0.48f;
		r->colorB[0] = 0.24f; r->colorB[1] = 0.52f; r->colorB[2] = 0.26f;
		r->colorC[0] = 0.68f; r->colorC[1] = 0.62f; r->colorC[2] = 0.54f;
	}

	int normalId = AddNode(ProcNodeType::NormalMap, ImVec2(580.0f, 300.0f)).id;
	FindNode(normalId)->paramA = 6.0f;

	int roughLevelsId = AddNode(ProcNodeType::Levels, ImVec2(580.0f, 480.0f)).id;
	FindNode(roughLevelsId)->paramA = 0.15f;
	FindNode(roughLevelsId)->paramB = 1.6f;

	int outId = AddNode(ProcNodeType::Output, ImVec2(850.0f, 180.0f)).id;

	AddLink(contNoiseId, 0, heightBlendId, 0);
	AddLink(craterId, 0, heightBlendId, 1);
	AddLink(heightBlendId, 0, colorRampId, 0);
	AddLink(heightBlendId, 0, normalId, 0);
	AddLink(heightBlendId, 0, roughLevelsId, 0);

	AddLink(colorRampId, 0, outId, 0);
	AddLink(normalId, 0, outId, 1);
	AddLink(roughLevelsId, 0, outId, 2);

	strcpy_s(s_exportNameBuf, "proc_planet");
	s_selectedNodeId = outId;
	s_graphDirty = true;
}

// =====================================================================================
// DirectX 12 HLSL Compute Shader Pipeline & Node-to-HLSL Code Generator
// =====================================================================================
struct ProcCBData {
	uint32_t width;
	uint32_t height;
	int32_t  mappingSpace;
	int32_t  seed;
	float    paramA;
	float    paramB;
	float    paramC;
	float    paramD;
	float    colorA[4];
	float    colorB[4];
	float    scaleX;
	float    scaleY;
	uint32_t hasInUV;
	uint32_t hasInA;
	uint32_t hasInB;
	uint32_t pad0;
	uint32_t pad1;
	uint32_t pad2;
};

static const char* GetProcHlslHeaderLibrary() {
	return R"HLSL(
cbuffer ProcCB : register(b0) {
	uint   g_Width;
	uint   g_Height;
	int    g_MappingSpace;
	int    g_Seed;
	float  g_ParamA;
	float  g_ParamB;
	float  g_ParamC;
	float  g_ParamD;
	float4 g_ColorA;
	float4 g_ColorB;
	float  g_ScaleX;
	float  g_ScaleY;
	uint   g_HasInUV;
	uint   g_HasInA;
	uint   g_HasInB;
	uint   g_Pad0;
	uint   g_Pad1;
	uint   g_Pad2;
};

StructuredBuffer<float4>   g_InUV      : register(t0);
StructuredBuffer<float4>   g_InA       : register(t1);
StructuredBuffer<float4>   g_InB       : register(t2);
RWStructuredBuffer<float4> g_OutAlbedo : register(u0);
RWStructuredBuffer<float4> g_OutNormal : register(u1);
RWStructuredBuffer<float4> g_OutARM    : register(u2);

static const float PROC_PI = 3.14159265f;

float Luminance(float3 c) {
	return dot(c, float3(0.299f, 0.587f, 0.114f));
}

float4 SampleBufferBilinearWrap(StructuredBuffer<float4> buf, float2 uv) {
	float u = frac(uv.x);
	if (u < 0.0f) u += 1.0f;
	float v = frac(uv.y);
	if (v < 0.0f) v += 1.0f;
	float fx = u * (float)g_Width - 0.5f;
	float fy = v * (float)g_Height - 0.5f;
	int x0 = (int)floor(fx);
	int y0 = (int)floor(fy);
	float tx = fx - (float)x0;
	float ty = fy - (float)y0;
	int w = (int)g_Width;
	int h = (int)g_Height;
	int x0w = ((x0 % w) + w) % w;
	int x1w = (((x0 + 1) % w) + w) % w;
	int y0w = ((y0 % h) + h) % h;
	int y1w = (((y0 + 1) % h) + h) % h;
	float4 c00 = buf[y0w * w + x0w];
	float4 c10 = buf[y0w * w + x1w];
	float4 c01 = buf[y1w * w + x0w];
	float4 c11 = buf[y1w * w + x1w];
	return lerp(lerp(c00, c10, tx), lerp(c01, c11, tx), ty);
}

float4 SampleInputUV(float2 uv) { return (g_HasInUV != 0) ? SampleBufferBilinearWrap(g_InUV, uv) : float4(uv.x, uv.y, 0.0f, 1.0f); }
float4 SampleInputA(float2 uv)  { return (g_HasInA  != 0) ? SampleBufferBilinearWrap(g_InA,  uv) : float4(0.0f, 0.0f, 0.0f, 1.0f); }
float4 SampleInputB(float2 uv)  { return (g_HasInB  != 0) ? SampleBufferBilinearWrap(g_InB,  uv) : float4(0.0f, 0.0f, 0.0f, 1.0f); }

float3 MapUVTo3D(float2 uv, int spaceMode) {
	if (spaceMode == 1) {
		float phi   = uv.x * 2.0f * PROC_PI;
		float theta = saturate(uv.y) * PROC_PI;
		float sinT  = sin(theta);
		return float3(sinT * cos(phi), cos(theta), sinT * sin(phi));
	} else if (spaceMode == 2) {
		float phi   = uv.x * 2.0f * PROC_PI;
		float theta = saturate(uv.y) * PROC_PI;
		float sinT  = sin(theta);
		float3 d    = float3(sinT * cos(phi), cos(theta), sinT * sin(phi));
		float maxA  = max(max(abs(d.x), abs(d.y)), max(abs(d.z), 0.0001f));
		return d / maxA;
	}
	return float3(uv.x, uv.y, 0.0f);
}

float2 Dir3DToSphericalUV(float3 dir) {
	float len = length(dir);
	if (len < 0.0001f) return float2(0.0f, 0.0f);
	float3 n = dir / len;
	float phi = atan2(n.z, n.x);
	if (phi < 0.0f) phi += 2.0f * PROC_PI;
	float u = frac(phi / (2.0f * PROC_PI));
	float v = saturate(acos(clamp(n.y, -1.0f, 1.0f)) / PROC_PI);
	return float2(u, v);
}

uint Hash2D(int x, int y, int seed) {
	uint h = (uint)(x * 374761393 + y * 668265263 + seed * 1442695041);
	h = (h ^ (h >> 13)) * 1274126177u;
	return h ^ (h >> 16);
}

uint Hash3D(int x, int y, int z, int seed) {
	uint h = (uint)(x * 374761393 + y * 668265263 + z * 2147483647 + seed * 1442695041);
	h = (h ^ (h >> 13)) * 1274126177u;
	return h ^ (h >> 16);
}

float FadePerlin(float t) {
	return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

float GradientNoise2D(float x, float y, int periodX, int periodY, int seed) {
	int px = max(1, periodX);
	int py = max(1, periodY);
	int ix = (int)floor(x);
	int iy = (int)floor(y);
	float fx = x - (float)ix;
	float fy = y - (float)iy;
	int x0 = ((ix % px) + px) % px;
	int y0 = ((iy % py) + py) % py;
	int x1 = (x0 + 1) % px;
	int y1 = (y0 + 1) % py;

	float u = FadePerlin(fx);
	float v = FadePerlin(fy);

	uint h00 = Hash2D(x0, y0, seed);
	uint h10 = Hash2D(x1, y0, seed);
	uint h01 = Hash2D(x0, y1, seed);
	uint h11 = Hash2D(x1, y1, seed);

	float a00 = (float)(h00 & 0xFFFFu) / 65535.0f * 2.0f * PROC_PI;
	float a10 = (float)(h10 & 0xFFFFu) / 65535.0f * 2.0f * PROC_PI;
	float a01 = (float)(h01 & 0xFFFFu) / 65535.0f * 2.0f * PROC_PI;
	float a11 = (float)(h11 & 0xFFFFu) / 65535.0f * 2.0f * PROC_PI;

	float d00 = cos(a00) * fx + sin(a00) * fy;
	float d10 = cos(a10) * (fx - 1.0f) + sin(a10) * fy;
	float d01 = cos(a01) * fx + sin(a01) * (fy - 1.0f);
	float d11 = cos(a11) * (fx - 1.0f) + sin(a11) * (fy - 1.0f);

	return lerp(lerp(d00, d10, u), lerp(d01, d11, u), v);
}

float Grad3D(uint h, float dx, float dy, float dz) {
	float theta = (float)(h & 0xFFFFu) / 65535.0f * 2.0f * PROC_PI;
	float z = ((float)((h >> 16) & 0xFFFFu) / 65535.0f) * 2.0f - 1.0f;
	float r = sqrt(max(0.0f, 1.0f - z * z));
	return (r * cos(theta)) * dx + (r * sin(theta)) * dy + z * dz;
}

float GradientNoise3D(float x, float y, float z, int seed) {
	int ix = (int)floor(x);
	int iy = (int)floor(y);
	int iz = (int)floor(z);
	float fx = x - (float)ix;
	float fy = y - (float)iy;
	float fz = z - (float)iz;
	float u = FadePerlin(fx);
	float v = FadePerlin(fy);
	float w = FadePerlin(fz);

	float n000 = Grad3D(Hash3D(ix,     iy,     iz,     seed), fx,        fy,        fz);
	float n100 = Grad3D(Hash3D(ix + 1, iy,     iz,     seed), fx - 1.0f, fy,        fz);
	float n010 = Grad3D(Hash3D(ix,     iy + 1, iz,     seed), fx,        fy - 1.0f, fz);
	float n110 = Grad3D(Hash3D(ix + 1, iy + 1, iz,     seed), fx - 1.0f, fy - 1.0f, fz);
	float n001 = Grad3D(Hash3D(ix,     iy,     iz + 1, seed), fx,        fy,        fz - 1.0f);
	float n101 = Grad3D(Hash3D(ix + 1, iy,     iz + 1, seed), fx - 1.0f, fy,        fz - 1.0f);
	float n011 = Grad3D(Hash3D(ix,     iy + 1, iz + 1, seed), fx,        fy - 1.0f, fz - 1.0f);
	float n111 = Grad3D(Hash3D(ix + 1, iy + 1, iz + 1, seed), fx - 1.0f, fy - 1.0f, fz - 1.0f);

	float nx00 = lerp(n000, n100, u);
	float nx10 = lerp(n010, n110, u);
	float nx01 = lerp(n001, n101, u);
	float nx11 = lerp(n011, n111, u);
	return lerp(lerp(nx00, nx10, v), lerp(nx01, nx11, v), w);
}

float Node_PerlinFBM(float2 uv, float3 pos3D, float scaleX, float scaleY, int octaves, float persistence, float lacunarity, int seed, bool ridged) {
	float total = 0.0f;
	float amplitude = 1.0f;
	float maxAmp = 0.0f;
	float freqX = max(1.0f, round(scaleX));
	float freqY = max(1.0f, round(scaleY));
	[loop]
	for (int i = 0; i < octaves; i++) {
		float n = 0.0f;
		if (g_MappingSpace == 0) {
			int px = max(1, (int)round(freqX));
			int py = max(1, (int)round(freqY));
			n = GradientNoise2D(uv.x * (float)px, uv.y * (float)py, px, py, seed + i * 101);
		} else {
			n = GradientNoise3D(pos3D.x * freqX * 0.5f, pos3D.y * freqY * 0.5f, pos3D.z * freqX * 0.5f, seed + i * 101);
		}
		if (ridged) {
			n = 1.0f - abs(n * 1.414f);
			n = saturate(n);
			n = n * n;
		} else {
			n = saturate(n * 0.7071f + 0.5f);
		}
		total += n * amplitude;
		maxAmp += amplitude;
		amplitude *= persistence;
		freqX *= lacunarity;
		freqY *= lacunarity;
	}
	return (maxAmp > 0.0f) ? saturate(total / maxAmp) : 0.0f;
}

float Node_Voronoi(float2 uv, float3 pos3D, float scale, float jitter, int mode, int seed, bool invertFlag) {
	float f1 = 1e9f;
	float f2 = 1e9f;
	float bestCellVal = 0.0f;
	if (g_MappingSpace == 0) {
		int period = max(1, (int)round(scale));
		float x = uv.x * (float)period;
		float y = uv.y * (float)period;
		int ix = (int)floor(x);
		int iy = (int)floor(y);
		for (int dy = -1; dy <= 1; dy++) {
			for (int dx = -1; dx <= 1; dx++) {
				int cx = ((ix + dx) % period + period) % period;
				int cy = ((iy + dy) % period + period) % period;
				uint h = Hash2D(cx, cy, seed);
				float rx = (float)(h & 0xFFFFu) / 65535.0f;
				float ry = (float)((h >> 16) & 0xFFFFu) / 65535.0f;
				float px = (float)(ix + dx) + 0.5f + (rx - 0.5f) * jitter;
				float py = (float)(iy + dy) + 0.5f + (ry - 0.5f) * jitter;
				float dist = length(float2(px - x, py - y));
				if (dist < f1) {
					f2 = f1;
					f1 = dist;
					bestCellVal = (float)((h >> 8) & 0xFFFFu) / 65535.0f;
				} else if (dist < f2) {
					f2 = dist;
				}
			}
		}
	} else {
		float freq = max(1.0f, scale) * 0.5f;
		float3 sp = pos3D * freq;
		int ix = (int)floor(sp.x);
		int iy = (int)floor(sp.y);
		int iz = (int)floor(sp.z);
		for (int dz = -1; dz <= 1; dz++) {
			for (int dy = -1; dy <= 1; dy++) {
				for (int dx = -1; dx <= 1; dx++) {
					int cx = ix + dx;
					int cy = iy + dy;
					int cz = iz + dz;
					uint h1 = Hash3D(cx, cy, cz, seed);
					uint h2 = Hash3D(cx, cy, cz, seed + 7919);
					float rx = (float)(h1 & 0xFFFFu) / 65535.0f;
					float ry = (float)((h1 >> 16) & 0xFFFFu) / 65535.0f;
					float rz = (float)(h2 & 0xFFFFu) / 65535.0f;
					float3 fp = float3(cx, cy, cz) + 0.5f + (float3(rx, ry, rz) - 0.5f) * jitter;
					float dist = length(fp - sp);
					if (dist < f1) {
						f2 = f1;
						f1 = dist;
						bestCellVal = (float)((h1 >> 8) & 0xFFFFu) / 65535.0f;
					} else if (dist < f2) {
						f2 = dist;
					}
				}
			}
		}
	}
	float val = saturate(f1);
	if (mode == 1) val = saturate(f2 - f1);
	else if (mode == 2) val = saturate(bestCellVal);
	return invertFlag ? (1.0f - val) : val;
}

float4 Node_Checker(float2 uv, float scaleX, float scaleY, bool gridMode, float lineWidth, float3 colorA, float3 colorB) {
	float sy = uv.y * max(1.0f, scaleY);
	float effScaleX = max(1.0f, scaleX);
	if (g_MappingSpace == 1) {
		float rowCenterV = (floor(sy) + 0.5f) / max(1.0f, scaleY);
		float sinLat = sin(saturate(rowCenterV) * PROC_PI);
		effScaleX = max(2.0f, round(effScaleX * 2.0f * sinLat));
	}
	float sx = uv.x * effScaleX;
	float3 col = colorA;
	if (gridMode) {
		float fx = frac(sx); if (fx < 0.0f) fx += 1.0f;
		float fy = frac(sy); if (fy < 0.0f) fy += 1.0f;
		float lw = saturate(lineWidth * 0.5f);
		bool isLine = (fx < lw || fx > 1.0f - lw || fy < lw || fy > 1.0f - lw);
		col = isLine ? colorB : colorA;
	} else {
		int cx = (int)floor(sx);
		int cy = (int)floor(sy);
		col = (((cx + cy) & 1) == 0) ? colorA : colorB;
	}
	return float4(col, 1.0f);
}

float4 Node_Brick(float2 uv, float scaleX, float scaleY, float mortar, float offset, float variation, int seed, float3 colorBrick, float3 colorMortar) {
	float cols = max(1.0f, round(scaleX));
	float rows = max(1.0f, round(scaleY));
	float m = saturate(mortar);
	float ry = uv.y * rows;
	int rowIdx = (int)floor(ry);
	float effCols = cols;
	if (g_MappingSpace == 1) {
		float rowCenterV = ((float)rowIdx + 0.5f) / rows;
		float sinLat = sin(saturate(rowCenterV) * PROC_PI);
		effCols = max(1.0f, round(cols * 2.0f * sinLat));
	}
	float rx = uv.x * effCols + (((rowIdx & 1) != 0) ? offset : 0.0f);
	int colIdx = (int)floor(rx);
	float fx = frac(rx); if (fx < 0.0f) fx += 1.0f;
	float fy = frac(ry); if (fy < 0.0f) fy += 1.0f;
	float mx = m * 0.5f;
	float my = m * (cols / rows) * 0.5f;
	if (fx < mx || fx > 1.0f - mx || fy < my || fy > 1.0f - my) {
		return float4(colorMortar, 1.0f);
	}
	uint bh = Hash2D(colIdx % (int)cols, rowIdx % (int)rows, seed);
	float var = (((float)(bh & 0xFFFFu) / 65535.0f) - 0.5f) * 2.0f * variation;
	return float4(saturate(colorBrick + var), 1.0f);
}

float4 Node_Gradient(float2 uv, float3 pos3D, int mode, float angleDeg, float repeatScale, float3 colorA, float3 colorB) {
	float rad = angleDeg * PROC_PI / 180.0f;
	float ca = cos(rad);
	float sa = sin(rad);
	float t = 0.0f;
	if (g_MappingSpace != 0) {
		if (mode == 0) {
			float proj = (-pos3D.y * ca + pos3D.x * sa) * 0.5f + 0.5f;
			t = (repeatScale <= 1.05f) ? saturate(proj * repeatScale) : frac(proj * repeatScale);
		} else {
			float d = length(pos3D.xz);
			t = saturate(d * max(0.1f, repeatScale));
		}
	} else if (mode == 0) {
		float2 d = uv - 0.5f;
		t = frac(((d.x * ca - d.y * sa) + 0.5f) * max(0.1f, repeatScale));
	} else {
		float2 d = uv - 0.5f;
		t = saturate(length(d) * 2.0f * max(0.1f, repeatScale));
	}
	return float4(lerp(colorA, colorB, t), 1.0f);
}

float4 Node_Blend(float4 a, float4 b, float opacity, float maskVal, int mode) {
	float factor = saturate(opacity) * maskVal;
	float3 res = b.rgb;
	if (mode == 1) res = a.rgb * b.rgb;
	else if (mode == 2) res = saturate(a.rgb + b.rgb);
	else if (mode == 3) res = saturate(a.rgb - b.rgb);
	else if (mode == 4) res = 1.0f - (1.0f - a.rgb) * (1.0f - b.rgb);
	else if (mode == 5) res = (a.rgb < 0.5f) ? (2.0f * a.rgb * b.rgb) : (1.0f - 2.0f * (1.0f - a.rgb) * (1.0f - b.rgb));
	else if (mode == 6) res = max(a.rgb, b.rgb);
	else if (mode == 7) res = min(a.rgb, b.rgb);
	return float4(saturate(lerp(a.rgb, res, factor)), 1.0f);
}

float4 Node_ColorRamp(float t, bool threeStops, float stop0, float stop1, float stop2, float3 colA, float3 colB, float3 colC) {
	float p0 = saturate(stop0);
	float p1 = max(p0, saturate(stop1));
	float p2 = max(p1, saturate(stop2));
	if (!threeStops) {
		float f = saturate((t - p0) / max(0.0001f, p2 - p0));
		return float4(lerp(colA, colC, f), 1.0f);
	}
	if (t <= p1) {
		float f = saturate((t - p0) / max(0.0001f, p1 - p0));
		return float4(lerp(colA, colB, f), 1.0f);
	}
	float f = saturate((t - p1) / max(0.0001f, p2 - p1));
	return float4(lerp(colB, colC, f), 1.0f);
}

float4 Node_Levels(float4 inCol, float brightness, float contrast, float gammaVal, bool invertFlag) {
	float3 v = inCol.rgb;
	if (invertFlag) v = 1.0f - v;
	v = saturate((v - 0.5f) * contrast + 0.5f + brightness);
	float invGamma = 1.0f / max(0.05f, gammaVal);
	v = saturate(pow(max(v, 0.0f), invGamma));
	return float4(v, 1.0f);
}
#line 1 "user_proc.hlsl"
)HLSL";
}

static const char* GetProcHlslFooterCSMain() {
	return R"HLSL(
[numthreads(8, 8, 1)]
void CSMain(uint3 dtid : SV_DispatchThreadID) {
	if (dtid.x >= g_Width || dtid.y >= g_Height) return;
	uint idx = dtid.y * g_Width + dtid.x;
	float2 baseUV = (float2(dtid.xy) + 0.5f) / float2(g_Width, g_Height);
	float2 uv = baseUV;
	float3 pos3D = float3(0.0f, 0.0f, 0.0f);
	if (g_HasInUV != 0) {
		float4 uvData = g_InUV[idx];
		if (uvData.w > 1.5f) {
			pos3D = (uvData.xyz - 0.5f) * 4.0f;
			uv = Dir3DToSphericalUV(pos3D);
		} else {
			uv = uvData.xy;
			pos3D = MapUVTo3D(uv, g_MappingSpace);
		}
	} else {
		pos3D = MapUVTo3D(uv, g_MappingSpace);
	}

	float4 outAlbedo = float4(0.5f, 0.5f, 0.5f, 1.0f);
	float4 outNormal = float4(0.5f, 0.5f, 1.0f, 1.0f);
	float4 outARM    = float4(1.0f, 0.6f, 0.0f, 1.0f);
	EvaluateProceduralTexture(dtid.xy, uv, pos3D, outAlbedo, outNormal, outARM);
	g_OutAlbedo[idx] = saturate(outAlbedo);
	g_OutNormal[idx] = saturate(outNormal);
	g_OutARM[idx]    = saturate(outARM);
}
)HLSL";
}

static bool EnsureComputeRootSignature(ID3D12Device* dev, std::string& outErrorLog) {
	if (s_computeRootSig) return true;

	D3D12_ROOT_PARAMETER rootParams[7] = {};
	rootParams[0].ParameterType            = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	rootParams[0].Constants.ShaderRegister = 0;
	rootParams[0].Constants.RegisterSpace  = 0;
	rootParams[0].Constants.Num32BitValues = 24;
	rootParams[0].ShaderVisibility         = D3D12_SHADER_VISIBILITY_ALL;
	for (int i = 0; i < 3; i++) {
		rootParams[1 + i].ParameterType             = D3D12_ROOT_PARAMETER_TYPE_SRV;
		rootParams[1 + i].Descriptor.ShaderRegister = static_cast<UINT>(i);
		rootParams[1 + i].Descriptor.RegisterSpace  = 0;
		rootParams[1 + i].ShaderVisibility          = D3D12_SHADER_VISIBILITY_ALL;
	}
	for (int i = 0; i < 3; i++) {
		rootParams[4 + i].ParameterType             = D3D12_ROOT_PARAMETER_TYPE_UAV;
		rootParams[4 + i].Descriptor.ShaderRegister = static_cast<UINT>(i);
		rootParams[4 + i].Descriptor.RegisterSpace  = 0;
		rootParams[4 + i].ShaderVisibility          = D3D12_SHADER_VISIBILITY_ALL;
	}

	D3D12_ROOT_SIGNATURE_DESC rsDesc = {};
	rsDesc.NumParameters     = 7;
	rsDesc.pParameters       = rootParams;
	rsDesc.NumStaticSamplers = 0;
	rsDesc.pStaticSamplers   = nullptr;
	rsDesc.Flags             = D3D12_ROOT_SIGNATURE_FLAG_NONE;

	ComPtr<ID3DBlob> sigBlob;
	ComPtr<ID3DBlob> errBlob;
	HRESULT hr = D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, &sigBlob, &errBlob);
	if (FAILED(hr)) {
		if (errBlob) {
			outErrorLog = std::string("RootSignature Error: ") + static_cast<const char*>(errBlob->GetBufferPointer());
		}
		return false;
	}
	hr = dev->CreateRootSignature(0, sigBlob->GetBufferPointer(), sigBlob->GetBufferSize(), IID_PPV_ARGS(&s_computeRootSig));
	return SUCCEEDED(hr);
}

static bool RunComputeShaderHLSL(
	const std::string& userHlslCode,
	int width, int height,
	const ProcCBData& cbData,
	const std::vector<float>* inUV,
	const std::vector<float>* inA,
	const std::vector<float>* inB,
	std::vector<float>& outAlbedo,
	std::vector<float>* outNormal,
	std::vector<float>* outARM,
	std::string& outErrorLog)
{
	ID3D12Device* dev = Renderer::GetDeviceDX12();
	if (!dev) {
		outErrorLog = "Error: D3D12 Device is not available.";
		return false;
	}
	if (!EnsureComputeRootSignature(dev, outErrorLog)) {
		return false;
	}

	std::string fullSource = GetProcHlslHeaderLibrary();
	fullSource += "\n";
	fullSource += userHlslCode;
	if (userHlslCode.find("CSMain") == std::string::npos) {
		fullSource += "\n";
		fullSource += GetProcHlslFooterCSMain();
	}

	ComPtr<ID3D12PipelineState> pso;
	auto it = s_computePsoCache.find(fullSource);
	if (it != s_computePsoCache.end()) {
		pso = it->second;
	} else {
		ComPtr<ID3DBlob> csBlob;
		ComPtr<ID3DBlob> errBlob;
		UINT compileFlags = D3DCOMPILE_OPTIMIZATION_LEVEL3;
		HRESULT hr = D3DCompile(
			fullSource.c_str(),
			fullSource.size(),
			"user_proc.hlsl",
			nullptr,
			nullptr,
			"CSMain",
			"cs_5_0",
			compileFlags,
			0,
			&csBlob,
			&errBlob
		);
		if (FAILED(hr)) {
			if (errBlob && errBlob->GetBufferPointer()) {
				outErrorLog = static_cast<const char*>(errBlob->GetBufferPointer());
			} else {
				outErrorLog = "D3DCompile failed with HRESULT: " + std::to_string(hr);
			}
			return false;
		}
		D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.pRootSignature = s_computeRootSig.Get();
		psoDesc.CS = { csBlob->GetBufferPointer(), csBlob->GetBufferSize() };
		hr = dev->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&pso));
		if (FAILED(hr)) {
			outErrorLog = "CreateComputePipelineState failed.";
			return false;
		}
		s_computePsoCache[fullSource] = pso;
	}

	size_t numPixels = static_cast<size_t>(width) * height;
	size_t bufBytes  = numPixels * 4 * sizeof(float);

	auto makeHeapProps = [](D3D12_HEAP_TYPE type) {
		D3D12_HEAP_PROPERTIES hp = {};
		hp.Type                 = type;
		hp.CPUPageProperty      = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		hp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		hp.CreationNodeMask     = 1;
		hp.VisibleNodeMask      = 1;
		return hp;
	};
	auto makeBufferDesc = [](UINT64 byteSize, D3D12_RESOURCE_FLAGS flags) {
		D3D12_RESOURCE_DESC rd = {};
		rd.Dimension          = D3D12_RESOURCE_DIMENSION_BUFFER;
		rd.Alignment          = 0;
		rd.Width              = byteSize;
		rd.Height             = 1;
		rd.DepthOrArraySize   = 1;
		rd.MipLevels          = 1;
		rd.Format             = DXGI_FORMAT_UNKNOWN;
		rd.SampleDesc.Count   = 1;
		rd.SampleDesc.Quality = 0;
		rd.Layout             = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		rd.Flags              = flags;
		return rd;
	};

	D3D12_HEAP_PROPERTIES uploadHeap   = makeHeapProps(D3D12_HEAP_TYPE_UPLOAD);
	D3D12_HEAP_PROPERTIES defaultHeap  = makeHeapProps(D3D12_HEAP_TYPE_DEFAULT);
	D3D12_HEAP_PROPERTIES readbackHeap = makeHeapProps(D3D12_HEAP_TYPE_READBACK);

	D3D12_RESOURCE_DESC srvBufDesc = makeBufferDesc(bufBytes, D3D12_RESOURCE_FLAG_NONE);
	D3D12_RESOURCE_DESC uavBufDesc = makeBufferDesc(bufBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	D3D12_RESOURCE_DESC rbBufDesc  = makeBufferDesc(bufBytes, D3D12_RESOURCE_FLAG_NONE);

	auto createUploadBuffer = [&](const std::vector<float>* srcData, ComPtr<ID3D12Resource>& outRes) -> bool {
		if (FAILED(dev->CreateCommittedResource(&uploadHeap, D3D12_HEAP_FLAG_NONE, &srvBufDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&outRes)))) {
			return false;
		}
		if (srcData && srcData->size() >= numPixels * 4) {
			void* mapped = nullptr;
			if (SUCCEEDED(outRes->Map(0, nullptr, &mapped))) {
				memcpy(mapped, srcData->data(), bufBytes);
				outRes->Unmap(0, nullptr);
			}
		}
		return true;
	};

	ComPtr<ID3D12Resource> resInUV, resInA, resInB;
	ComPtr<ID3D12Resource> resUavAlbedo, resUavNormal, resUavARM;
	ComPtr<ID3D12Resource> resRbAlbedo, resRbNormal, resRbARM;

	if (!createUploadBuffer(inUV, resInUV) ||
	    !createUploadBuffer(inA,  resInA)  ||
	    !createUploadBuffer(inB,  resInB)) {
		outErrorLog = "Failed to allocate D3D12 Upload buffers.";
		return false;
	}

	if (FAILED(dev->CreateCommittedResource(&defaultHeap, D3D12_HEAP_FLAG_NONE, &uavBufDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&resUavAlbedo))) ||
	    FAILED(dev->CreateCommittedResource(&defaultHeap, D3D12_HEAP_FLAG_NONE, &uavBufDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&resUavNormal))) ||
	    FAILED(dev->CreateCommittedResource(&defaultHeap, D3D12_HEAP_FLAG_NONE, &uavBufDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&resUavARM)))) {
		outErrorLog = "Failed to allocate D3D12 UAV buffers.";
		return false;
	}

	if (FAILED(dev->CreateCommittedResource(&readbackHeap, D3D12_HEAP_FLAG_NONE, &rbBufDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&resRbAlbedo))) ||
	    FAILED(dev->CreateCommittedResource(&readbackHeap, D3D12_HEAP_FLAG_NONE, &rbBufDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&resRbNormal))) ||
	    FAILED(dev->CreateCommittedResource(&readbackHeap, D3D12_HEAP_FLAG_NONE, &rbBufDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&resRbARM)))) {
		outErrorLog = "Failed to allocate D3D12 Readback buffers.";
		return false;
	}

	ComPtr<ID3D12CommandAllocator> cmdAlloc;
	ComPtr<ID3D12GraphicsCommandList> cmdList;
	if (FAILED(dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&cmdAlloc))) ||
	    FAILED(dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, cmdAlloc.Get(), pso.Get(), IID_PPV_ARGS(&cmdList)))) {
		outErrorLog = "Failed to create D3D12 CommandList.";
		return false;
	}

	cmdList->SetComputeRootSignature(s_computeRootSig.Get());
	cmdList->SetComputeRoot32BitConstants(0, 24, &cbData, 0);
	cmdList->SetComputeRootShaderResourceView(1, resInUV->GetGPUVirtualAddress());
	cmdList->SetComputeRootShaderResourceView(2, resInA->GetGPUVirtualAddress());
	cmdList->SetComputeRootShaderResourceView(3, resInB->GetGPUVirtualAddress());
	cmdList->SetComputeRootUnorderedAccessView(4, resUavAlbedo->GetGPUVirtualAddress());
	cmdList->SetComputeRootUnorderedAccessView(5, resUavNormal->GetGPUVirtualAddress());
	cmdList->SetComputeRootUnorderedAccessView(6, resUavARM->GetGPUVirtualAddress());

	UINT groupsX = static_cast<UINT>((width + 7) / 8);
	UINT groupsY = static_cast<UINT>((height + 7) / 8);
	cmdList->Dispatch(groupsX, groupsY, 1);

	auto makeTransition = [](ID3D12Resource* res, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after) {
		D3D12_RESOURCE_BARRIER b = {};
		b.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		b.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		b.Transition.pResource   = res;
		b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		b.Transition.StateBefore = before;
		b.Transition.StateAfter  = after;
		return b;
	};
	D3D12_RESOURCE_BARRIER barriers[3] = {
		makeTransition(resUavAlbedo.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE),
		makeTransition(resUavNormal.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE),
		makeTransition(resUavARM.Get(),    D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE)
	};
	cmdList->ResourceBarrier(3, barriers);

	cmdList->CopyResource(resRbAlbedo.Get(), resUavAlbedo.Get());
	cmdList->CopyResource(resRbNormal.Get(), resUavNormal.Get());
	cmdList->CopyResource(resRbARM.Get(),    resUavARM.Get());

	cmdList->Close();

	ID3D12CommandList* lists[] = { cmdList.Get() };
	Renderer::GetCommandQueueDX12()->ExecuteCommandLists(1, lists);
	Renderer::FlushGPUDX12();

	auto readbackToVector = [&](ComPtr<ID3D12Resource>& rbRes, std::vector<float>& dstVec) {
		dstVec.resize(numPixels * 4);
		void* mapped = nullptr;
		D3D12_RANGE readRange = { 0, bufBytes };
		if (SUCCEEDED(rbRes->Map(0, &readRange, &mapped))) {
			memcpy(dstVec.data(), mapped, bufBytes);
			D3D12_RANGE writeRange = { 0, 0 };
			rbRes->Unmap(0, &writeRange);
		}
	};

	readbackToVector(resRbAlbedo, outAlbedo);
	if (outNormal) readbackToVector(resRbNormal, *outNormal);
	if (outARM)    readbackToVector(resRbARM,    *outARM);

	outErrorLog = "Compiled & executed successfully on GPU (cs_5_0, " + std::to_string(width) + "x" + std::to_string(height) + ").";
	return true;
}

static std::string LoadHlslFileContent(const std::string& filePath) {
	std::ifstream ifs(filePath);
	if (!ifs.is_open()) return "";
	std::stringstream ss;
	ss << ifs.rdbuf();
	return ss.str();
}

static bool SaveHlslToShaderFolder(const std::string& rawFileName, const std::string& code, std::string& outSavedPath) {
	std::string name = rawFileName;
	if (name.empty()) name = "proc_generated";
	if (name.size() < 5 || name.substr(name.size() - 5) != ".hlsl") {
		name += ".hlsl";
	}
	fs::path p = fs::path("shader") / name;
	if (p.has_parent_path()) {
		std::error_code ec;
		fs::create_directories(p.parent_path(), ec);
	}
	std::ofstream ofs(p);
	if (!ofs.is_open()) return false;
	ofs << code;
	outSavedPath = p.generic_string();
	return true;
}

static void EnsureDefaultCustomHlslFile() {
	fs::path defaultPath = fs::path("shader") / "proc_custom_swirl.hlsl";
	std::error_code ec;
	if (fs::exists(defaultPath, ec)) return;

	std::string sampleCode = R"HLSL(// ============================================================================
// Procedural Texture HLSL (DirectX 12 Compute Shader: cs_5_0)
// - Available Inputs:
//   uint2  px     : Pixel coordinate (0 .. g_Width-1, 0 .. g_Height-1)
//   float2 uv     : Normalized UV (0..1) or warped UV from UV input pin
//   float3 pos3D  : 3D position on Sphere / Box (when 3D Mapping Space is active)
// - Available Uniforms (from Custom HLSL Node or Editor):
//   g_ParamA..D, g_ColorA, g_ColorB, g_ScaleX, g_ScaleY, g_Seed, g_MappingSpace
// - Available Input Samplers:
//   SampleInputA(uv), SampleInputB(uv), SampleInputUV(uv)
// ============================================================================

void EvaluateProceduralTexture(
	uint2 px,
	float2 uv,
	float3 pos3D,
	out float4 outAlbedo,
	out float4 outNormal,
	out float4 outARM)
{
	// Center coordinates (-1 .. +1)
	float2 centered = (uv - 0.5f) * 2.0f;
	float radius = length(centered);
	float angle  = atan2(centered.y, centered.x);

	// Swirl / Spiral distortion controlled by g_ParamA & g_ScaleX
	float swirlStrength = g_ParamA * 6.0f;
	float spiral = sin(angle * max(1.0f, round(g_ScaleX)) + radius * swirlStrength - Node_PerlinFBM(uv, pos3D, g_ScaleX, g_ScaleY, 4, 0.5f, 2.0f, g_Seed, false) * 4.0f);
	float mask = saturate(spiral * 0.5f + 0.5f);

	// Blend with optional In A pin if connected
	float4 inColA = SampleInputA(uv);
	float3 baseCol = lerp(g_ColorA.rgb, g_ColorB.rgb, mask);
	if (g_HasInA != 0) {
		baseCol = lerp(baseCol, inColA.rgb, saturate(g_ParamB));
	}

	outAlbedo = float4(baseCol, 1.0f);

	// Simple analytical normal from height mask
	float nx = ddx_fine(mask); // fallback safe calculation
	outNormal = float4(0.5f, 0.5f, 1.0f, 1.0f);
	outARM    = float4(1.0f, lerp(0.25f, 0.85f, 1.0f - mask), 0.0f, 1.0f);
}
)HLSL";
	// Remove ddx_fine comment line and keep pure CS-compatible code
	size_t ddxPos = sampleCode.find("\tfloat nx = ddx_fine");
	if (ddxPos != std::string::npos) {
		size_t endLine = sampleCode.find('\n', ddxPos);
		if (endLine != std::string::npos) sampleCode.erase(ddxPos, endLine - ddxPos + 1);
	}
	std::string savedPath;
	SaveHlslToShaderFolder("proc_custom_swirl.hlsl", sampleCode, savedPath);
}

static std::string GenerateHLSLFromGraph() {
	// Topological sort over all nodes in s_nodes
	std::unordered_set<int> visited;
	std::vector<int> order;
	std::function<void(int)> dfs = [&](int nodeId) {
		if (visited.count(nodeId)) return;
		visited.insert(nodeId);
		for (const auto& l : s_links) {
			if (l.toNodeId == nodeId) {
				dfs(l.fromNodeId);
			}
		}
		order.push_back(nodeId);
	};
	for (const auto& n : s_nodes) {
		dfs(n.id);
	}

	std::ostringstream ss;
	ss << std::fixed << std::setprecision(4);
	ss << "// ============================================================================\n";
	ss << "// Auto-Generated HLSL from Procedural Texture Node Graph\n";
	ss << "// Compatible with DirectX 12 Compute Shader (cs_5_0) & Custom HLSL Node\n";
	ss << "// ============================================================================\n\n";

	// Helper lambda to evaluate graph at any (uv, pos3D) so NormalMap can sample neighbors
	ss << "void EvaluateNodeGraphAt(float2 uv, float3 pos3D, out float4 outAlbedo, out float4 outARM, out float outHeightForNormal, out float outNormalStrength, out float outNormalSignY, out bool outHasNormalNode)\n";
	ss << "{\n";
	ss << "\toutHasNormalNode = false;\n";
	ss << "\toutHeightForNormal = 0.5f;\n";
	ss << "\toutNormalStrength = 3.0f;\n";
	ss << "\toutNormalSignY = 1.0f;\n\n";

	for (int id : order) {
		ProcNode* node = FindNode(id);
		if (!node || node->type == ProcNodeType::Output) continue;

		NodeMeta meta = GetNodeMeta(node->type);
		ss << "\t// --- Node #" << node->id << ": " << meta.title << " ---\n";

		auto getUvExpr = [&](int pinIdx) -> std::string {
			ProcNode* src = GetInputSourceNode(node->id, pinIdx);
			if (src) return "n" + std::to_string(src->id) + ".xy";
			return "uv";
		};
		auto getPos3DExpr = [&](int pinIdx) -> std::string {
			ProcNode* src = GetInputSourceNode(node->id, pinIdx);
			if (src && src->type == ProcNodeType::UVWarp) {
				return "MapUVTo3D(n" + std::to_string(src->id) + ".xy, g_MappingSpace)";
			}
			return "pos3D";
		};

		switch (node->type) {
		case ProcNodeType::PerlinNoise: {
			ss << "\tfloat v" << id << " = Node_PerlinFBM(" << getUvExpr(0) << ", " << getPos3DExpr(0) << ", "
			   << node->scaleX << "f, " << node->scaleY << "f, " << node->octaves << ", "
			   << node->persistence << "f, " << node->lacunarity << "f, " << node->seed << ", "
			   << (node->flagA ? "true" : "false") << ");\n";
			ss << "\tfloat4 n" << id << " = float4(v" << id << ", v" << id << ", v" << id << ", 1.0f);\n\n";
			break;
		}
		case ProcNodeType::Voronoi: {
			ss << "\tfloat v" << id << " = Node_Voronoi(" << getUvExpr(0) << ", " << getPos3DExpr(0) << ", "
			   << node->scaleX << "f, " << node->paramA << "f, " << node->mode << ", " << node->seed << ", "
			   << (node->flagA ? "true" : "false") << ");\n";
			ss << "\tfloat4 n" << id << " = float4(v" << id << ", v" << id << ", v" << id << ", 1.0f);\n\n";
			break;
		}
		case ProcNodeType::Checker: {
			ss << "\tfloat4 n" << id << " = Node_Checker(" << getUvExpr(0) << ", "
			   << node->scaleX << "f, " << node->scaleY << "f, " << (node->flagA ? "true" : "false") << ", "
			   << node->paramA << "f, float3(" << node->colorA[0] << "f, " << node->colorA[1] << "f, " << node->colorA[2] << "f), "
			   << "float3(" << node->colorB[0] << "f, " << node->colorB[1] << "f, " << node->colorB[2] << "f));\n\n";
			break;
		}
		case ProcNodeType::Brick: {
			ss << "\tfloat4 n" << id << " = Node_Brick(" << getUvExpr(0) << ", "
			   << node->scaleX << "f, " << node->scaleY << "f, " << node->paramA << "f, "
			   << node->paramB << "f, " << node->paramC << "f, " << node->seed << ", "
			   << "float3(" << node->colorA[0] << "f, " << node->colorA[1] << "f, " << node->colorA[2] << "f), "
			   << "float3(" << node->colorB[0] << "f, " << node->colorB[1] << "f, " << node->colorB[2] << "f));\n\n";
			break;
		}
		case ProcNodeType::Gradient: {
			ss << "\tfloat4 n" << id << " = Node_Gradient(" << getUvExpr(0) << ", " << getPos3DExpr(0) << ", "
			   << node->mode << ", " << node->paramA << "f, " << node->scaleX << "f, "
			   << "float3(" << node->colorA[0] << "f, " << node->colorA[1] << "f, " << node->colorA[2] << "f), "
			   << "float3(" << node->colorB[0] << "f, " << node->colorB[1] << "f, " << node->colorB[2] << "f));\n\n";
			break;
		}
		case ProcNodeType::ConstantColor: {
			ss << "\tfloat4 n" << id << " = float4("
			   << node->colorA[0] << "f, " << node->colorA[1] << "f, " << node->colorA[2] << "f, 1.0f);\n\n";
			break;
		}
		case ProcNodeType::Blend: {
			ProcNode* baseN = GetInputSourceNode(id, 0);
			ProcNode* blendN = GetInputSourceNode(id, 1);
			ProcNode* maskN = GetInputSourceNode(id, 2);
			std::string aExpr = baseN ? ("n" + std::to_string(baseN->id)) : "float4(0.0f, 0.0f, 0.0f, 1.0f)";
			std::string bExpr = blendN ? ("n" + std::to_string(blendN->id)) : "float4(0.0f, 0.0f, 0.0f, 1.0f)";
			std::string mExpr = maskN ? ("Luminance(n" + std::to_string(maskN->id) + ".rgb)") : "1.0f";
			ss << "\tfloat4 n" << id << " = Node_Blend(" << aExpr << ", " << bExpr << ", "
			   << node->paramA << "f, " << mExpr << ", " << node->mode << ");\n\n";
			break;
		}
		case ProcNodeType::ColorRamp: {
			ProcNode* inN = GetInputSourceNode(id, 0);
			std::string tExpr = inN ? ("Luminance(n" + std::to_string(inN->id) + ".rgb)") : "0.0f";
			ss << "\tfloat4 n" << id << " = Node_ColorRamp(" << tExpr << ", " << (node->flagA ? "true" : "false") << ", "
			   << node->paramD << "f, " << node->paramB << "f, " << node->paramC << "f, "
			   << "float3(" << node->colorA[0] << "f, " << node->colorA[1] << "f, " << node->colorA[2] << "f), "
			   << "float3(" << node->colorB[0] << "f, " << node->colorB[1] << "f, " << node->colorB[2] << "f), "
			   << "float3(" << node->colorC[0] << "f, " << node->colorC[1] << "f, " << node->colorC[2] << "f));\n\n";
			break;
		}
		case ProcNodeType::UVWarp: {
			ProcNode* srcN = GetInputSourceNode(id, 0);
			ProcNode* warpN = GetInputSourceNode(id, 1);
			std::string wExpr = warpN ? ("(n" + std::to_string(warpN->id) + ".xy - 0.5f) * " + std::to_string(node->paramA) + "f") : "float2(0.0f, 0.0f)";
			if (srcN) {
				ss << "\tfloat4 n" << id << " = n" << srcN->id << ";\n\n";
			} else {
				ss << "\tfloat2 wuv" << id << " = frac(uv + " << wExpr << ");\n";
				ss << "\tfloat4 n" << id << " = float4(wuv" << id << ".x, wuv" << id << ".y, 0.0f, 1.0f);\n\n";
			}
			break;
		}
		case ProcNodeType::Levels: {
			ProcNode* inN = GetInputSourceNode(id, 0);
			std::string inExpr = inN ? ("n" + std::to_string(inN->id)) : "float4(0.0f, 0.0f, 0.0f, 1.0f)";
			ss << "\tfloat4 n" << id << " = Node_Levels(" << inExpr << ", "
			   << node->paramA << "f, " << node->paramB << "f, " << node->paramC << "f, "
			   << (node->flagA ? "true" : "false") << ");\n\n";
			break;
		}
		case ProcNodeType::NormalMap: {
			ProcNode* hN = GetInputSourceNode(id, 0);
			std::string hExpr = hN ? ("Luminance(n" + std::to_string(hN->id) + ".rgb)") : "0.5f";
			ss << "\toutHasNormalNode = true;\n";
			ss << "\toutHeightForNormal = " << hExpr << ";\n";
			ss << "\toutNormalStrength = " << node->paramA << "f;\n";
			ss << "\toutNormalSignY = " << (node->flagA ? "-1.0f" : "1.0f") << ";\n";
			ss << "\tfloat4 n" << id << " = float4(0.5f, 0.5f, 1.0f, 1.0f);\n\n";
			break;
		}
		case ProcNodeType::CustomHLSL: {
			ProcNode* inA = GetInputSourceNode(id, 1);
			std::string fbExpr = inA ? ("n" + std::to_string(inA->id)) : "float4(0.5f, 0.5f, 0.5f, 1.0f)";
			ss << "\t// CustomHLSL Node (" << node->customHlslFile << ")\n";
			ss << "\tfloat4 n" << id << " = " << fbExpr << ";\n\n";
			break;
		}
		default:
			break;
		}
	}

	ProcNode* outNode = FindOutputNode();
	ProcNode* albedoSrc = outNode ? GetInputSourceNode(outNode->id, 0) : nullptr;
	ProcNode* roughSrc  = outNode ? GetInputSourceNode(outNode->id, 2) : nullptr;
	ProcNode* metalSrc  = outNode ? GetInputSourceNode(outNode->id, 3) : nullptr;
	ProcNode* aoSrc     = outNode ? GetInputSourceNode(outNode->id, 4) : nullptr;

	float defaultRough = outNode ? outNode->paramA : 0.6f;
	float defaultMetal = outNode ? outNode->paramB : 0.0f;
	float defaultAO    = outNode ? outNode->paramC : 1.0f;

	ss << "\t// --- Composite Output ---\n";
	if (albedoSrc) {
		ss << "\toutAlbedo = n" << albedoSrc->id << ";\n";
	} else {
		ss << "\toutAlbedo = float4(0.5f, 0.5f, 0.5f, 1.0f);\n";
	}
	ss << "\tfloat ao    = " << (aoSrc    ? ("Luminance(n" + std::to_string(aoSrc->id)    + ".rgb)") : (std::to_string(defaultAO)    + "f")) << ";\n";
	ss << "\tfloat rough = " << (roughSrc ? ("Luminance(n" + std::to_string(roughSrc->id) + ".rgb)") : (std::to_string(defaultRough) + "f")) << ";\n";
	ss << "\tfloat metal = " << (metalSrc ? ("Luminance(n" + std::to_string(metalSrc->id) + ".rgb)") : (std::to_string(defaultMetal) + "f")) << ";\n";
	ss << "\toutARM = float4(ao, rough, metal, 1.0f);\n";
	ss << "}\n\n";

	ss << "void EvaluateProceduralTexture(\n";
	ss << "\tuint2 px,\n";
	ss << "\tfloat2 uv,\n";
	ss << "\tfloat3 pos3D,\n";
	ss << "\tout float4 outAlbedo,\n";
	ss << "\tout float4 outNormal,\n";
	ss << "\tout float4 outARM)\n";
	ss << "{\n";
	ss << "\tfloat hCenter, normStrength, normSignY;\n";
	ss << "\tbool hasNormal;\n";
	ss << "\tEvaluateNodeGraphAt(uv, pos3D, outAlbedo, outARM, hCenter, normStrength, normSignY, hasNormal);\n";
	ss << "\toutNormal = float4(0.5f, 0.5f, 1.0f, 1.0f);\n";
	ss << "\tif (hasNormal) {\n";
	ss << "\t\tfloat2 texel = 1.0f / float2(max(1u, g_Width), max(1u, g_Height));\n";
	ss << "\t\tfloat4 dummyA, dummyARM;\n";
	ss << "\t\tfloat hL, hR, hU, hD, dummyS, dummyY;\n";
	ss << "\t\tbool dummyHas;\n";
	ss << "\t\tfloat2 uvL = frac(uv + float2(-texel.x, 0.0f));\n";
	ss << "\t\tfloat2 uvR = frac(uv + float2( texel.x, 0.0f));\n";
	ss << "\t\tfloat2 uvU = frac(uv + float2(0.0f, -texel.y));\n";
	ss << "\t\tfloat2 uvD = frac(uv + float2(0.0f,  texel.y));\n";
	ss << "\t\tEvaluateNodeGraphAt(uvL, MapUVTo3D(uvL, g_MappingSpace), dummyA, dummyARM, hL, dummyS, dummyY, dummyHas);\n";
	ss << "\t\tEvaluateNodeGraphAt(uvR, MapUVTo3D(uvR, g_MappingSpace), dummyA, dummyARM, hR, dummyS, dummyY, dummyHas);\n";
	ss << "\t\tEvaluateNodeGraphAt(uvU, MapUVTo3D(uvU, g_MappingSpace), dummyA, dummyARM, hU, dummyS, dummyY, dummyHas);\n";
	ss << "\t\tEvaluateNodeGraphAt(uvD, MapUVTo3D(uvD, g_MappingSpace), dummyA, dummyARM, hD, dummyS, dummyY, dummyHas);\n";
	ss << "\t\tfloat dX = (hR - hL) * 4.0f * normStrength;\n";
	ss << "\t\tfloat dY = (hD - hU) * 4.0f * normStrength * normSignY;\n";
	ss << "\t\tfloat3 n = normalize(float3(-dX, -dY, 1.0f));\n";
	ss << "\t\toutNormal = float4(saturate(n * 0.5f + 0.5f), 1.0f);\n";
	ss << "\t}\n";
	ss << "}\n";

	return ss.str();
}

static void ConvertFloatToByteRGBA(const std::vector<float>& srcFloat, std::vector<unsigned char>& dstBytes);

static bool EvaluateHlslCodeOnGPU(int width, int height, bool updateGpuPreviews) {
	ProcCBData cb = {};
	cb.width        = static_cast<uint32_t>(width);
	cb.height       = static_cast<uint32_t>(height);
	cb.mappingSpace = s_mappingSpace;
	cb.seed         = 1337;
	cb.paramA       = 0.5f;
	cb.paramB       = 0.5f;
	cb.paramC       = 0.5f;
	cb.paramD       = 0.5f;
	cb.colorA[0] = 0.2f; cb.colorA[1] = 0.5f; cb.colorA[2] = 0.9f; cb.colorA[3] = 1.0f;
	cb.colorB[0] = 0.95f; cb.colorB[1] = 0.75f; cb.colorB[2] = 0.2f; cb.colorB[3] = 1.0f;
	cb.scaleX       = 4.0f;
	cb.scaleY       = 4.0f;

	std::vector<float> albedo, normal, arm;
	std::string logMsg;
	bool ok = RunComputeShaderHLSL(
		s_hlslCodeText,
		width, height,
		cb,
		nullptr, nullptr, nullptr,
		albedo, &normal, &arm,
		logMsg
	);
	s_hlslCompileLog = logMsg;
	s_hlslLastCompileSuccess = ok;
	if (!ok) return false;

	s_outAlbedoRGBA = std::move(albedo);
	s_outNormalRGBA = std::move(normal);
	s_outArmRGBA    = std::move(arm);

	if (updateGpuPreviews) {
		std::vector<unsigned char> byteBuf;
		ConvertFloatToByteRGBA(s_outAlbedoRGBA, byteBuf);
		s_outAlbedoTex = Texture::UpdateOrCreateFromRGBA("__proc_out_albedo", byteBuf.data(), width, height);

		ConvertFloatToByteRGBA(s_outNormalRGBA, byteBuf);
		s_outNormalTex = Texture::UpdateOrCreateFromRGBA("__proc_out_normal", byteBuf.data(), width, height);

		ConvertFloatToByteRGBA(s_outArmRGBA, byteBuf);
		s_outArmTex = Texture::UpdateOrCreateFromRGBA("__proc_out_arm", byteBuf.data(), width, height);
		s_preview3DDirty = true;
	}
	return true;
}

static void EvaluateSingleNode(ProcNode& node, int width, int height) {
	node.evalWidth = width;
	node.evalHeight = height;
	node.bufferRGBA.resize(static_cast<size_t>(width) * height * 4);

	auto getCoords3D = [&](int x, int y, ProcNode* uvSource, float& u, float& v, float& px, float& py, float& pz) {
		float baseU = (static_cast<float>(x) + 0.5f) / static_cast<float>(width);
		float baseV = (static_cast<float>(y) + 0.5f) / static_cast<float>(height);
		if (uvSource && static_cast<int>(uvSource->bufferRGBA.size()) == width * height * 4) {
			const float* p = &uvSource->bufferRGBA[(y * width + x) * 4];
			if (p[3] > 1.5f) {
				px = (p[0] - 0.5f) * 4.0f;
				py = (p[1] - 0.5f) * 4.0f;
				pz = (p[2] - 0.5f) * 4.0f;
				Dir3DToSphericalUV(px, py, pz, u, v);
				return;
			}
			u = p[0];
			v = p[1];
		} else {
			u = baseU;
			v = baseV;
		}
		MapUVTo3D(u, v, s_mappingSpace, px, py, pz);
	};
	auto getUV = [&](int x, int y, ProcNode* uvSource, float& u, float& v) {
		if (uvSource && static_cast<int>(uvSource->bufferRGBA.size()) == width * height * 4) {
			const float* p = &uvSource->bufferRGBA[(y * width + x) * 4];
			u = p[0];
			v = p[1];
		} else {
			u = (static_cast<float>(x) + 0.5f) / static_cast<float>(width);
			v = (static_cast<float>(y) + 0.5f) / static_cast<float>(height);
		}
	};


	switch (node.type) {
	case ProcNodeType::PerlinNoise: {
		ProcNode* uvNode = GetInputSourceNode(node.id, 0);
		for (int y = 0; y < height; y++) {
			for (int x = 0; x < width; x++) {
				float u, v;
				getUV(x, y, uvNode, u, v);
				float px, py, pz;
				getCoords3D(x, y, uvNode, u, v, px, py, pz);
				float val = (s_mappingSpace == 0)
					? EvaluateFBM(u, v, node.scaleX, node.scaleY, node.octaves, node.persistence, node.lacunarity, node.seed, node.flagA)
					: EvaluateFBM3D(px, py, pz, node.scaleX, node.scaleY, node.octaves, node.persistence, node.lacunarity, node.seed, node.flagA);
				float* dst = &node.bufferRGBA[(y * width + x) * 4];
				dst[0] = dst[1] = dst[2] = val;
				dst[3] = 1.0f;
			}
		}
		break;
	}
	case ProcNodeType::Voronoi: {
		ProcNode* uvNode = GetInputSourceNode(node.id, 0);
		for (int y = 0; y < height; y++) {
			for (int x = 0; x < width; x++) {
				float u, v;
				getUV(x, y, uvNode, u, v);
				float px, py, pz;
				getCoords3D(x, y, uvNode, u, v, px, py, pz);
				float val = (s_mappingSpace == 0)
					? EvaluateVoronoi(u, v, node.scaleX, node.paramA, node.mode, node.seed)
					: EvaluateVoronoi3D(px, py, pz, node.scaleX, node.paramA, node.mode, node.seed);
				if (node.flagA) val = 1.0f - val;
				float* dst = &node.bufferRGBA[(y * width + x) * 4];
				dst[0] = dst[1] = dst[2] = val;
				dst[3] = 1.0f;
			}
		}
		break;
	}
	case ProcNodeType::Checker: {
		ProcNode* uvNode = GetInputSourceNode(node.id, 0);
		for (int y = 0; y < height; y++) {
			for (int x = 0; x < width; x++) {
				float u, v;
				getUV(x, y, uvNode, u, v);

				float sy = v * std::max(1.0f, node.scaleY);
				float effScaleX = std::max(1.0f, node.scaleX);
				if (s_mappingSpace == 1) {
					float rowCenterV = (std::floor(sy) + 0.5f) / std::max(1.0f, node.scaleY);
					float sinLat = std::sin(Clamp01(rowCenterV) * 3.14159265f);
					effScaleX = std::max(2.0f, std::round(effScaleX * 2.0f * sinLat));
				}
				float sx = u * effScaleX;
				const float* col = node.colorA;
				if (node.flagA) {
					// Grid mode
					float fx = Frac(sx);
					float fy = Frac(sy);
					float lw = Clamp01(node.paramA * 0.5f);
					bool isLine = (fx < lw || fx > 1.0f - lw || fy < lw || fy > 1.0f - lw);
					col = isLine ? node.colorB : node.colorA;
				} else {
					int cx = static_cast<int>(std::floor(sx));
					int cy = static_cast<int>(std::floor(sy));
					col = (((cx + cy) & 1) == 0) ? node.colorA : node.colorB;
				}
				float* dst = &node.bufferRGBA[(y * width + x) * 4];
				dst[0] = col[0]; dst[1] = col[1]; dst[2] = col[2]; dst[3] = 1.0f;
			}
		}
		break;
	}
	case ProcNodeType::Brick: {
		ProcNode* uvNode = GetInputSourceNode(node.id, 0);
		float cols = std::max(1.0f, std::round(node.scaleX));
		float rows = std::max(1.0f, std::round(node.scaleY));
		float mortar = Clamp01(node.paramA);
		for (int y = 0; y < height; y++) {
			for (int x = 0; x < width; x++) {
				float u, v;
				getUV(x, y, uvNode, u, v);
				float ry = v * rows;
				int rowIdx = static_cast<int>(std::floor(ry));
				float effCols = cols;
				if (s_mappingSpace == 1) {
					float rowCenterV = (static_cast<float>(rowIdx) + 0.5f) / rows;
					float sinLat = std::sin(Clamp01(rowCenterV) * 3.14159265f);
					effCols = std::max(1.0f, std::round(cols * 2.0f * sinLat));
				}
				float rx = u * effCols + ((rowIdx & 1) ? node.paramB : 0.0f);
				int colIdx = static_cast<int>(std::floor(rx));
				float fx = Frac(rx);
				float fy = Frac(ry);

				float mx = mortar * 0.5f;
				float my = mortar * (cols / rows) * 0.5f;
				bool isMortar = (fx < mx || fx > 1.0f - mx || fy < my || fy > 1.0f - my);

				float* dst = &node.bufferRGBA[(y * width + x) * 4];
				if (isMortar) {
					dst[0] = node.colorB[0];
					dst[1] = node.colorB[1];
					dst[2] = node.colorB[2];
				} else {
					uint32_t bh = Hash2D(colIdx % static_cast<int>(cols), rowIdx % static_cast<int>(rows), node.seed);
					float var = ((static_cast<float>(bh & 0xFFFFu) / 65535.0f) - 0.5f) * 2.0f * node.paramC;
					dst[0] = Clamp01(node.colorA[0] + var);
					dst[1] = Clamp01(node.colorA[1] + var);
					dst[2] = Clamp01(node.colorA[2] + var);
				}
				dst[3] = 1.0f;
			}
		}
		break;
	}
	case ProcNodeType::Gradient: {
		ProcNode* uvNode = GetInputSourceNode(node.id, 0);
		float rad = node.paramA * 3.14159265f / 180.0f;
		float ca = std::cos(rad);
		float sa = std::sin(rad);
		for (int y = 0; y < height; y++) {
			for (int x = 0; x < width; x++) {
				float u, v;
				getUV(x, y, uvNode, u, v);
				float t = 0.0f;
				float px, py, pz;
				getCoords3D(x, y, uvNode, u, v, px, py, pz);
				if (s_mappingSpace != 0) {
					if (node.mode == 0) {
						float proj = (-py * ca + px * sa) * 0.5f + 0.5f;
						t = (node.scaleX <= 1.05f) ? Clamp01(proj * node.scaleX) : Frac(proj * node.scaleX);
					} else {
						float d = std::sqrt(px * px + pz * pz);
						t = Clamp01(d * std::max(0.1f, node.scaleX));
					}
				} else if (node.mode == 0) {
					float du = u - 0.5f;
					float dv = v - 0.5f;
					t = Frac(((du * ca - dv * sa) + 0.5f) * std::max(0.1f, node.scaleX));
				} else {
					float du = u - 0.5f;
					float dv = v - 0.5f;
					t = Clamp01(std::sqrt(du * du + dv * dv) * 2.0f * std::max(0.1f, node.scaleX));
				}
				float* dst = &node.bufferRGBA[(y * width + x) * 4];
				for (int c = 0; c < 3; c++) {
					dst[c] = Lerp(node.colorA[c], node.colorB[c], t);
				}
				dst[3] = 1.0f;
			}
		}
		break;
	}
	case ProcNodeType::ConstantColor: {
		for (int i = 0; i < width * height; i++) {
			float* dst = &node.bufferRGBA[i * 4];
			dst[0] = node.colorA[0];
			dst[1] = node.colorA[1];
			dst[2] = node.colorA[2];
			dst[3] = 1.0f;
		}
		break;
	}
	case ProcNodeType::Blend: {
		ProcNode* baseNode = GetInputSourceNode(node.id, 0);
		ProcNode* blendNode = GetInputSourceNode(node.id, 1);
		ProcNode* maskNode = GetInputSourceNode(node.id, 2);

		for (int i = 0; i < width * height; i++) {
			float a[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
			float b[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
			if (baseNode && static_cast<int>(baseNode->bufferRGBA.size()) == width * height * 4) {
				for (int c = 0; c < 4; c++) a[c] = baseNode->bufferRGBA[i * 4 + c];
			}
			if (blendNode && static_cast<int>(blendNode->bufferRGBA.size()) == width * height * 4) {
				for (int c = 0; c < 4; c++) b[c] = blendNode->bufferRGBA[i * 4 + c];
			}
			float factor = Clamp01(node.paramA);
			if (maskNode && static_cast<int>(maskNode->bufferRGBA.size()) == width * height * 4) {
				factor *= Luminance(&maskNode->bufferRGBA[i * 4]);
			}

			float* dst = &node.bufferRGBA[i * 4];
			for (int c = 0; c < 3; c++) {
				float res = b[c];
				switch (node.mode) {
				case 0: res = b[c]; break; // Mix
				case 1: res = a[c] * b[c]; break; // Multiply
				case 2: res = Clamp01(a[c] + b[c]); break; // Add
				case 3: res = Clamp01(a[c] - b[c]); break; // Subtract
				case 4: res = 1.0f - (1.0f - a[c]) * (1.0f - b[c]); break; // Screen
				case 5: res = (a[c] < 0.5f) ? (2.0f * a[c] * b[c]) : (1.0f - 2.0f * (1.0f - a[c]) * (1.0f - b[c])); break; // Overlay
				case 6: res = std::max(a[c], b[c]); break; // Max
				case 7: res = std::min(a[c], b[c]); break; // Min
				}
				dst[c] = Clamp01(Lerp(a[c], res, factor));
			}
			dst[3] = 1.0f;
		}
		break;
	}
	case ProcNodeType::ColorRamp: {
		ProcNode* inNode = GetInputSourceNode(node.id, 0);
		float p0 = Clamp01(node.paramD);
		float p1 = Clamp01(node.paramB);
		float p2 = Clamp01(node.paramC);
		if (p1 < p0) p1 = p0;
		if (p2 < p1) p2 = p1;

		for (int i = 0; i < width * height; i++) {
			float t = 0.0f;
			if (inNode && static_cast<int>(inNode->bufferRGBA.size()) == width * height * 4) {
				t = Luminance(&inNode->bufferRGBA[i * 4]);
			}
			float* dst = &node.bufferRGBA[i * 4];
			if (!node.flagA) {
				// 2 stops (ColorA -> ColorC)
				float span = std::max(0.0001f, p2 - p0);
				float f = Clamp01((t - p0) / span);
				for (int c = 0; c < 3; c++) dst[c] = Lerp(node.colorA[c], node.colorC[c], f);
			} else {
				// 3 stops (ColorA -> ColorB -> ColorC)
				if (t <= p1) {
					float span = std::max(0.0001f, p1 - p0);
					float f = Clamp01((t - p0) / span);
					for (int c = 0; c < 3; c++) dst[c] = Lerp(node.colorA[c], node.colorB[c], f);
				} else {
					float span = std::max(0.0001f, p2 - p1);
					float f = Clamp01((t - p1) / span);
					for (int c = 0; c < 3; c++) dst[c] = Lerp(node.colorB[c], node.colorC[c], f);
				}
			}
			dst[3] = 1.0f;
		}
		break;
	}
	case ProcNodeType::UVWarp: {
		ProcNode* srcNode = GetInputSourceNode(node.id, 0);
		ProcNode* warpNode = GetInputSourceNode(node.id, 1);
		float strength = node.paramA;

		for (int y = 0; y < height; y++) {
			for (int x = 0; x < width; x++) {
				float u = (static_cast<float>(x) + 0.5f) / static_cast<float>(width);
				float v = (static_cast<float>(y) + 0.5f) / static_cast<float>(height);
				float dx = 0.0f, dy = 0.0f;
				if (warpNode && static_cast<int>(warpNode->bufferRGBA.size()) == width * height * 4) {
					const float* wp = &warpNode->bufferRGBA[(y * width + x) * 4];
					dx = (wp[0] - 0.5f) * strength;
					dy = (wp[1] - 0.5f) * strength;
				}
				float wu = Frac(u + dx);
				float wv = Frac(v + dy);
				float* dst = &node.bufferRGBA[(y * width + x) * 4];
				if (s_mappingSpace != 0) {
					float px, py, pz;
					MapUVTo3D(u, v, s_mappingSpace, px, py, pz);
					float dz = 0.0f;
					if (warpNode && static_cast<int>(warpNode->bufferRGBA.size()) == width * height * 4) {
						int xShift = (x + width / 3) % width;
						int yShift = (y + height / 3) % height;
						const float* wp2 = &warpNode->bufferRGBA[(yShift * width + xShift) * 4];
						dz = (wp2[0] - 0.5f) * strength;
					}
					float wpx = px + dx * 2.0f;
					float wpy = py + dy * 2.0f;
					float wpz = pz + dz * 2.0f;
					if (srcNode && static_cast<int>(srcNode->bufferRGBA.size()) == width * height * 4) {
						Dir3DToSphericalUV(wpx, wpy, wpz, wu, wv);
						SampleBilinearWrap(srcNode->bufferRGBA, width, height, wu, wv, dst);
					} else {
						dst[0] = wpx * 0.25f + 0.5f;
						dst[1] = wpy * 0.25f + 0.5f;
						dst[2] = wpz * 0.25f + 0.5f;
						dst[3] = 2.0f;
					}
				} else if (srcNode && static_cast<int>(srcNode->bufferRGBA.size()) == width * height * 4) {
					SampleBilinearWrap(srcNode->bufferRGBA, width, height, wu, wv, dst);
				} else {
					dst[0] = wu;
					dst[1] = wv;
					dst[2] = 0.0f;
					dst[3] = 1.0f;
				}
			}
		}
		break;
	}
	case ProcNodeType::Levels: {
		ProcNode* inNode = GetInputSourceNode(node.id, 0);
		float brightness = node.paramA;
		float contrast = node.paramB;
		float invGamma = 1.0f / std::max(0.05f, node.paramC);

		for (int i = 0; i < width * height; i++) {
			float* dst = &node.bufferRGBA[i * 4];
			for (int c = 0; c < 3; c++) {
				float v = (inNode && static_cast<int>(inNode->bufferRGBA.size()) == width * height * 4)
					? inNode->bufferRGBA[i * 4 + c] : 0.0f;
				if (node.flagA) v = 1.0f - v;
				v = (v - 0.5f) * contrast + 0.5f + brightness;
				v = Clamp01(v);
				v = std::pow(v, invGamma);
				dst[c] = Clamp01(v);
			}
			dst[3] = 1.0f;
		}
		break;
	}
	case ProcNodeType::NormalMap: {
		ProcNode* hNode = GetInputSourceNode(node.id, 0);
		float strength = node.paramA;
		float signY = node.flagA ? -1.0f : 1.0f;

		auto sampleH = [&](int x, int y) -> float {
			if (!hNode || static_cast<int>(hNode->bufferRGBA.size()) != width * height * 4) return 0.5f;
			if (s_mappingSpace == 1 && (y < 0 || y >= height)) {
				x += width / 2;
				y = (y < 0) ? -y : (2 * height - 1 - y);
				y = std::clamp(y, 0, height - 1);
			}
			int wx = ((x % width) + width) % width;
			int wy = ((y % height) + height) % height;
			return Luminance(&hNode->bufferRGBA[(wy * width + wx) * 4]);
		};

		for (int y = 0; y < height; y++) {
			for (int x = 0; x < width; x++) {
				float tl = sampleH(x - 1, y - 1);
				float tc = sampleH(x,     y - 1);
				float tr = sampleH(x + 1, y - 1);
				float ml = sampleH(x - 1, y);
				float mr = sampleH(x + 1, y);
				float bl = sampleH(x - 1, y + 1);
				float bc = sampleH(x,     y + 1);
				float br = sampleH(x + 1, y + 1);

				float dX = ((tr + 2.0f * mr + br) - (tl + 2.0f * ml + bl)) * strength;
				float dY = ((bl + 2.0f * bc + br) - (tl + 2.0f * tc + tr)) * strength * signY;
				float dZ = 1.0f;

				float len = std::sqrt(dX * dX + dY * dY + dZ * dZ);
				if (len > 0.0001f) {
					dX /= len; dY /= len; dZ /= len;
				}
				float* dst = &node.bufferRGBA[(y * width + x) * 4];
				dst[0] = Clamp01(-dX * 0.5f + 0.5f);
				dst[1] = Clamp01(-dY * 0.5f + 0.5f);
				dst[2] = Clamp01( dZ * 0.5f + 0.5f);
				dst[3] = 1.0f;
			}
		}
		break;
	}
	case ProcNodeType::CustomHLSL: {
		EnsureDefaultCustomHlslFile();
		ProcNode* uvNode  = GetInputSourceNode(node.id, 0);
		ProcNode* inANode = GetInputSourceNode(node.id, 1);
		ProcNode* inBNode = GetInputSourceNode(node.id, 2);

		std::string code = LoadHlslFileContent(node.customHlslFile);
		bool ok = false;
		if (!code.empty()) {
			ProcCBData cb = {};
			cb.width        = static_cast<uint32_t>(width);
			cb.height       = static_cast<uint32_t>(height);
			cb.mappingSpace = s_mappingSpace;
			cb.seed         = node.seed;
			cb.paramA       = node.paramA;
			cb.paramB       = node.paramB;
			cb.paramC       = node.paramC;
			cb.paramD       = node.paramD;
			for (int c = 0; c < 4; c++) {
				cb.colorA[c] = node.colorA[c];
				cb.colorB[c] = node.colorB[c];
			}
			cb.scaleX  = node.scaleX;
			cb.scaleY  = node.scaleY;
			cb.hasInUV = (uvNode  && static_cast<int>(uvNode->bufferRGBA.size())  == width * height * 4) ? 1u : 0u;
			cb.hasInA  = (inANode && static_cast<int>(inANode->bufferRGBA.size()) == width * height * 4) ? 1u : 0u;
			cb.hasInB  = (inBNode && static_cast<int>(inBNode->bufferRGBA.size()) == width * height * 4) ? 1u : 0u;

			std::string errLog;
			ok = RunComputeShaderHLSL(
				code, width, height, cb,
				cb.hasInUV ? &uvNode->bufferRGBA  : nullptr,
				cb.hasInA  ? &inANode->bufferRGBA : nullptr,
				cb.hasInB  ? &inBNode->bufferRGBA : nullptr,
				node.bufferRGBA, nullptr, nullptr, errLog
			);
			s_hlslCompileLog = "[" + node.customHlslFile + "] " + errLog;
			s_hlslLastCompileSuccess = ok;
		} else {
			s_hlslCompileLog = "Error: Could not read HLSL file: " + node.customHlslFile;
			s_hlslLastCompileSuccess = false;
		}
		if (!ok) {
			for (int y = 0; y < height; y++) {
				for (int x = 0; x < width; x++) {
					bool chk = (((x / 8) + (y / 8)) & 1) == 0;
					float* dst = &node.bufferRGBA[(y * width + x) * 4];
					dst[0] = chk ? 1.0f : 0.1f;
					dst[1] = 0.0f;
					dst[2] = chk ? 1.0f : 0.1f;
					dst[3] = 1.0f;
				}
			}
		}
		break;
	}
	case ProcNodeType::Output: {
		ProcNode* albedoNode = GetInputSourceNode(node.id, 0);
		for (int i = 0; i < width * height; i++) {
			float* dst = &node.bufferRGBA[i * 4];
			if (albedoNode && static_cast<int>(albedoNode->bufferRGBA.size()) == width * height * 4) {
				for (int c = 0; c < 4; c++) dst[c] = albedoNode->bufferRGBA[i * 4 + c];
			} else {
				dst[0] = 0.5f; dst[1] = 0.5f; dst[2] = 0.5f; dst[3] = 1.0f;
			}
		}
		break;
	}
	}
}

static void ConvertFloatToByteRGBA(const std::vector<float>& srcFloat, std::vector<unsigned char>& dstBytes) {
	dstBytes.resize(srcFloat.size());
	for (size_t i = 0; i < srcFloat.size(); i++) {
		dstBytes[i] = static_cast<unsigned char>(Clamp01(srcFloat[i]) * 255.0f + 0.5f);
	}
}

static void EvaluateGraph(int width, int height, bool updateGpuPreviews) {
	// Topological sort (DFS) over all nodes so every node's mini-preview is up to date
	std::unordered_set<int> visited;
	std::vector<int> order;

	std::function<void(int)> dfs = [&](int nodeId) {
		if (visited.count(nodeId)) return;
		visited.insert(nodeId);
		for (const auto& l : s_links) {
			if (l.toNodeId == nodeId) {
				dfs(l.fromNodeId);
			}
		}
		order.push_back(nodeId);
	};

	for (const auto& n : s_nodes) {
		dfs(n.id);
	}

	std::vector<unsigned char> byteBuf;
	for (int id : order) {
		ProcNode* node = FindNode(id);
		if (!node) continue;
		EvaluateSingleNode(*node, width, height);
		if (updateGpuPreviews) {
			ConvertFloatToByteRGBA(node->bufferRGBA, byteBuf);
			std::string key = "__proc_node_" + std::to_string(node->id);
			node->previewTexture = Texture::UpdateOrCreateFromRGBA(key, byteBuf.data(), width, height);
		}
	}

	// Build composite Output buffers (Albedo, Normal, ARM)
	size_t totalFloats = static_cast<size_t>(width) * height * 4;
	s_outAlbedoRGBA.assign(totalFloats, 1.0f);
	s_outNormalRGBA.assign(totalFloats, 1.0f);
	s_outArmRGBA.assign(totalFloats, 1.0f);

	ProcNode* outNode = FindOutputNode();
	ProcNode* albedoSrc = outNode ? GetInputSourceNode(outNode->id, 0) : nullptr;
	ProcNode* normalSrc = outNode ? GetInputSourceNode(outNode->id, 1) : nullptr;
	ProcNode* roughSrc  = outNode ? GetInputSourceNode(outNode->id, 2) : nullptr;
	ProcNode* metalSrc  = outNode ? GetInputSourceNode(outNode->id, 3) : nullptr;
	ProcNode* aoSrc     = outNode ? GetInputSourceNode(outNode->id, 4) : nullptr;

	float defaultRough = outNode ? outNode->paramA : 0.6f;
	float defaultMetal = outNode ? outNode->paramB : 0.0f;
	float defaultAO    = outNode ? outNode->paramC : 1.0f;

	for (int i = 0; i < width * height; i++) {
		// Albedo
		if (albedoSrc && static_cast<int>(albedoSrc->bufferRGBA.size()) == width * height * 4) {
			for (int c = 0; c < 4; c++) s_outAlbedoRGBA[i * 4 + c] = albedoSrc->bufferRGBA[i * 4 + c];
		} else {
			s_outAlbedoRGBA[i * 4 + 0] = 0.5f;
			s_outAlbedoRGBA[i * 4 + 1] = 0.5f;
			s_outAlbedoRGBA[i * 4 + 2] = 0.5f;
			s_outAlbedoRGBA[i * 4 + 3] = 1.0f;
		}

		// Normal
		if (normalSrc && static_cast<int>(normalSrc->bufferRGBA.size()) == width * height * 4) {
			for (int c = 0; c < 4; c++) s_outNormalRGBA[i * 4 + c] = normalSrc->bufferRGBA[i * 4 + c];
		} else {
			s_outNormalRGBA[i * 4 + 0] = 0.5f;
			s_outNormalRGBA[i * 4 + 1] = 0.5f;
			s_outNormalRGBA[i * 4 + 2] = 1.0f;
			s_outNormalRGBA[i * 4 + 3] = 1.0f;
		}

		// ARM (R=AO, G=Roughness, B=Metallic, A=1.0)
		float ao = (aoSrc && static_cast<int>(aoSrc->bufferRGBA.size()) == width * height * 4)
			? Luminance(&aoSrc->bufferRGBA[i * 4]) : defaultAO;
		float rough = (roughSrc && static_cast<int>(roughSrc->bufferRGBA.size()) == width * height * 4)
			? Luminance(&roughSrc->bufferRGBA[i * 4]) : defaultRough;
		float metal = (metalSrc && static_cast<int>(metalSrc->bufferRGBA.size()) == width * height * 4)
			? Luminance(&metalSrc->bufferRGBA[i * 4]) : defaultMetal;

		s_outArmRGBA[i * 4 + 0] = ao;
		s_outArmRGBA[i * 4 + 1] = rough;
		s_outArmRGBA[i * 4 + 2] = metal;
		s_outArmRGBA[i * 4 + 3] = 1.0f;
	}

	if (updateGpuPreviews) {
		ConvertFloatToByteRGBA(s_outAlbedoRGBA, byteBuf);
		s_outAlbedoTex = Texture::UpdateOrCreateFromRGBA("__proc_out_albedo", byteBuf.data(), width, height);

		ConvertFloatToByteRGBA(s_outNormalRGBA, byteBuf);
		s_outNormalTex = Texture::UpdateOrCreateFromRGBA("__proc_out_normal", byteBuf.data(), width, height);

		ConvertFloatToByteRGBA(s_outArmRGBA, byteBuf);
		s_outArmTex = Texture::UpdateOrCreateFromRGBA("__proc_out_arm", byteBuf.data(), width, height);
		s_preview3DDirty = true;
	}
}

// =====================================================================================
static void Render3DPreviewImage() {
	const int res = 160;
	const int srcW = std::max(1, s_previewResolution);
	const int srcH = std::max(1, s_previewResolution);

	std::vector<float> imgRGBA(static_cast<size_t>(res) * res * 4, 0.0f);

	float cy = std::cos(s_previewYaw), sy = std::sin(s_previewYaw);
	float cp = std::cos(s_previewPitch), sp = std::sin(s_previewPitch);

	auto worldToLocal = [&](float wx, float wy, float wz, float& lx, float& ly, float& lz) {
		float tx =  cy * wx + sy * wz;
		float ty =  wy;
		float tz = -sy * wx + cy * wz;
		lx = tx;
		ly =  cp * ty + sp * tz;
		lz = -sp * ty + cp * tz;
	};
	auto localToWorld = [&](float lx, float ly, float lz, float& wx, float& wy, float& wz) {
		float tx = lx;
		float ty = cp * ly - sp * lz;
		float tz = sp * ly + cp * lz;
		wx =  cy * tx - sy * tz;
		wy =  ty;
		wz =  sy * tx + cy * tz;
	};

	float lcy = std::cos(s_lightYaw), lsy = std::sin(s_lightYaw);
	float lcp = std::cos(s_lightPitch), lsp = std::sin(s_lightPitch);
	float lightX = lcp * lsy;
	float lightY = lsp;
	float lightZ = -lcp * lcy;
	{
		float lLen = std::sqrt(lightX * lightX + lightY * lightY + lightZ * lightZ);
		if (lLen > 0.0001f) { lightX /= lLen; lightY /= lLen; lightZ /= lLen; }
	}

	const std::vector<float>* activeChannelBuf = &s_outAlbedoRGBA;
	if (s_previewChannel == 1) activeChannelBuf = &s_outNormalRGBA;
	else if (s_previewChannel == 2) activeChannelBuf = &s_outArmRGBA;
	else if (s_previewChannel == 3) {
		ProcNode* sel = FindNode(s_selectedNodeId);
		if (sel && !sel->bufferRGBA.empty()) activeChannelBuf = &sel->bufferRGBA;
	}

	for (int py = 0; py < res; py++) {
		for (int px = 0; px < res; px++) {
			float ndcX = ((static_cast<float>(px) + 0.5f) / static_cast<float>(res)) * 2.0f - 1.0f;
			float ndcY = 1.0f - ((static_cast<float>(py) + 0.5f) / static_cast<float>(res)) * 2.0f;

			float* outPix = &imgRGBA[(py * res + px) * 4];

			float bgGrad = Clamp01((ndcY + 1.0f) * 0.5f);
			outPix[0] = Lerp(0.08f, 0.14f, bgGrad);
			outPix[1] = Lerp(0.09f, 0.16f, bgGrad);
			outPix[2] = Lerp(0.11f, 0.20f, bgGrad);
			outPix[3] = 1.0f;

			if (s_previewShapeMode == 3) {
				float fovScale = 0.85f;
				float rdx = ndcX * fovScale;
				float rdy = ndcY * fovScale;
				float rdz = 1.0f;
				float rLen = std::sqrt(rdx * rdx + rdy * rdy + rdz * rdz);
				rdx /= rLen; rdy /= rLen; rdz /= rLen;

				float lx, ly, lz;
				worldToLocal(rdx, rdy, rdz, lx, ly, lz);

				float u, v;
				Dir3DToSphericalUV(lx, ly, lz, u, v);

				float col[4];
				SampleBilinearWrap(*activeChannelBuf, srcW, srcH, u, v, col);
				outPix[0] = col[0];
				outPix[1] = col[1];
				outPix[2] = col[2];
				outPix[3] = 1.0f;
				continue;
			}

			float roX = 0.0f, roY = 0.0f, roZ = -2.35f;
			float rdX = ndcX * 0.48f;
			float rdY = ndcY * 0.48f;
			float rdZ = 1.0f;
			float rdLen = std::sqrt(rdX * rdX + rdY * rdY + rdZ * rdZ);
			rdX /= rdLen; rdY /= rdLen; rdZ /= rdLen;

			bool hit = false;
			float u = 0.0f, v = 0.0f;
			float wnX = 0.0f, wnY = 0.0f, wnZ = -1.0f;
			float wtX = 1.0f, wtY = 0.0f, wtZ = 0.0f;
			float wbX = 0.0f, wbY = 1.0f, wbZ = 0.0f;

			if (s_previewShapeMode == 1) {
				const float radius = 0.86f;
				float b = roX * rdX + roY * rdY + roZ * rdZ;
				float c = (roX * roX + roY * roY + roZ * roZ) - radius * radius;
				float disc = b * b - c;
				if (disc >= 0.0f) {
					float t = -b - std::sqrt(disc);
					if (t > 0.0f) {
						hit = true;
						float hx = (roX + rdX * t) / radius;
						float hy = (roY + rdY * t) / radius;
						float hz = (roZ + rdZ * t) / radius;
						wnX = hx; wnY = hy; wnZ = hz;

						float lx, ly, lz;
						worldToLocal(hx, hy, hz, lx, ly, lz);
						Dir3DToSphericalUV(lx, ly, lz, u, v);

						float ltx = -lz, lty = 0.0f, ltz = lx;
						float ltLen = std::sqrt(ltx * ltx + ltz * ltz);
						if (ltLen > 0.0001f) { ltx /= ltLen; ltz /= ltLen; }
						else { ltx = 1.0f; ltz = 0.0f; }

						float lbx = ly * ltz - lz * lty;
						float lby = lz * ltx - lx * ltz;
						float lbz = lx * lty - ly * ltx;

						localToWorld(ltx, lty, ltz, wtX, wtY, wtZ);
						localToWorld(lbx, lby, lbz, wbX, wbY, wbZ);
					}
				}
			} else if (s_previewShapeMode == 2) {
				const float boxHalf = 0.64f;
				float lroX, lroY, lroZ;
				float lrdX, lrdY, lrdZ;
				worldToLocal(roX, roY, roZ, lroX, lroY, lroZ);
				worldToLocal(rdX, rdY, rdZ, lrdX, lrdY, lrdZ);

				float tMin = -1e9f, tMax = 1e9f;
				int hitAxis = 0;
				float hitSign = 1.0f;

				const float roArr[3] = { lroX, lroY, lroZ };
				const float rdArr[3] = { lrdX, lrdY, lrdZ };
				bool boxValid = true;
				for (int a = 0; a < 3; a++) {
					if (std::fabs(rdArr[a]) < 1e-6f) {
						if (roArr[a] < -boxHalf || roArr[a] > boxHalf) { boxValid = false; break; }
					} else {
						float invD = 1.0f / rdArr[a];
						float t1 = (-boxHalf - roArr[a]) * invD;
						float t2 = ( boxHalf - roArr[a]) * invD;
						float s = -1.0f;
						if (t1 > t2) { std::swap(t1, t2); s = 1.0f; }
						if (t1 > tMin) { tMin = t1; hitAxis = a; hitSign = s; }
						if (t2 < tMax) { tMax = t2; }
						if (tMin > tMax) { boxValid = false; break; }
					}
				}

				if (boxValid && tMin > 0.0f) {
					hit = true;
					float lhx = lroX + lrdX * tMin;
					float lhy = lroY + lrdY * tMin;
					float lhz = lroZ + lrdZ * tMin;

					float lnx = 0.0f, lny = 0.0f, lnz = 0.0f;
					float ltx = 0.0f, lty = 0.0f, ltz = 0.0f;
					float lbx = 0.0f, lby = 0.0f, lbz = 0.0f;

					if (hitAxis == 0) {
						lnx = hitSign;
						u = (lhz * (-hitSign) / boxHalf) * 0.5f + 0.5f;
						v = (-lhy / boxHalf) * 0.5f + 0.5f;
						ltx = 0.0f; lty = 0.0f; ltz = -hitSign;
						lbx = 0.0f; lby = -1.0f; lbz = 0.0f;
					} else if (hitAxis == 1) {
						lny = hitSign;
						u = (lhx / boxHalf) * 0.5f + 0.5f;
						v = (lhz * hitSign / boxHalf) * 0.5f + 0.5f;
						ltx = 1.0f; lty = 0.0f; ltz = 0.0f;
						lbx = 0.0f; lby = 0.0f; lbz = hitSign;
					} else {
						lnz = hitSign;
						u = (lhx * hitSign / boxHalf) * 0.5f + 0.5f;
						v = (-lhy / boxHalf) * 0.5f + 0.5f;
						ltx = hitSign; lty = 0.0f; ltz = 0.0f;
						lbx = 0.0f; lby = -1.0f; lbz = 0.0f;
					}

					if (s_mappingSpace != 0) {
						Dir3DToSphericalUV(lhx, lhy, lhz, u, v);
					}

					localToWorld(lnx, lny, lnz, wnX, wnY, wnZ);
					localToWorld(ltx, lty, ltz, wtX, wtY, wtZ);
					localToWorld(lbx, lby, lbz, wbX, wbY, wbZ);
				}
			}

			if (!hit) continue;

			if (!s_previewPBRShading || s_previewChannel != 0) {
				float col[4];
				SampleBilinearWrap(*activeChannelBuf, srcW, srcH, u, v, col);
				outPix[0] = col[0];
				outPix[1] = col[1];
				outPix[2] = col[2];
				outPix[3] = 1.0f;
				continue;
			}

			float albedo[4], normSample[4], armSample[4];
			SampleBilinearWrap(s_outAlbedoRGBA, srcW, srcH, u, v, albedo);
			SampleBilinearWrap(s_outNormalRGBA, srcW, srcH, u, v, normSample);
			SampleBilinearWrap(s_outArmRGBA,    srcW, srcH, u, v, armSample);

			float ao        = Clamp01(armSample[0]);
			float roughness = std::clamp(armSample[1], 0.05f, 1.0f);
			float metallic  = Clamp01(armSample[2]);

			float tnx = normSample[0] * 2.0f - 1.0f;
			float tny = normSample[1] * 2.0f - 1.0f;
			float tnz = std::max(0.01f, normSample[2] * 2.0f - 1.0f);

			float nx = wtX * tnx + wbX * tny + wnX * tnz;
			float ny = wtY * tnx + wbY * tny + wnY * tnz;
			float nz = wtZ * tnx + wbZ * tny + wnZ * tnz;
			float nLen = std::sqrt(nx * nx + ny * ny + nz * nz);
			if (nLen > 0.0001f) { nx /= nLen; ny /= nLen; nz /= nLen; }

			float vx = -rdX, vy = -rdY, vz = -rdZ;
			float ndotl = std::max(0.0f, nx * lightX + ny * lightY + nz * lightZ);
			float ndotv = std::max(0.001f, nx * vx + ny * vy + nz * vz);

			float hx = lightX + vx, hy = lightY + vy, hz = lightZ + vz;
			float hLen = std::sqrt(hx * hx + hy * hy + hz * hz);
			if (hLen > 0.0001f) { hx /= hLen; hy /= hLen; hz /= hLen; }
			float ndoth = std::max(0.0f, nx * hx + ny * hy + nz * hz);
			float vdoth = std::max(0.0f, vx * hx + vy * hy + vz * hz);

			float alpha = roughness * roughness;
			float alpha2 = alpha * alpha;
			float denom = ndoth * ndoth * (alpha2 - 1.0f) + 1.0f;
			float D = alpha2 / std::max(0.0001f, 3.14159265f * denom * denom);
			float k = (roughness + 1.0f) * (roughness + 1.0f) * 0.125f;
			float G = (ndotv / (ndotv * (1.0f - k) + k)) * (ndotl / (ndotl * (1.0f - k) + k));
			float fresnelPow = std::pow(1.0f - vdoth, 5.0f);

			for (int c = 0; c < 3; c++) {
				float f0 = Lerp(0.04f, albedo[c], metallic);
				float F = f0 + (1.0f - f0) * fresnelPow;
				float spec = (D * G * F) / std::max(0.001f, 4.0f * ndotv * std::max(0.001f, ndotl));
				float kd = (1.0f - F) * (1.0f - metallic);
				float diff = kd * albedo[c] / 3.14159265f;

				float skyFactor = ny * 0.5f + 0.5f;
				float ambColor = Lerp(0.14f, 0.34f, skyFactor);
				float envSpec = Lerp(0.05f, 0.28f, (1.0f - roughness) * skyFactor) * f0;
				float ambient = (albedo[c] * (1.0f - metallic * 0.7f) * ambColor + envSpec) * ao;

				float lit = ambient + (diff + spec) * ndotl * 2.4f;
				lit = lit / (1.0f + lit * 0.22f);
				outPix[c] = Clamp01(lit);
			}
			outPix[3] = 1.0f;
		}
	}

	std::vector<unsigned char> byteBuf;
	ConvertFloatToByteRGBA(imgRGBA, byteBuf);
	s_preview3DTex = Texture::UpdateOrCreateFromRGBA("__proc_preview_3d", byteBuf.data(), res, res);
	s_preview3DDirty = false;
}

// PNG Export via DirectXTex
// =====================================================================================
static bool SaveRGBABufferToPNG(const std::vector<float>& floatRGBA, int width, int height, const std::string& filePath) {
	std::vector<unsigned char> byteRGBA;
	ConvertFloatToByteRGBA(floatRGBA, byteRGBA);

	DirectX::ScratchImage image;
	HRESULT hr = image.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM, width, height, 1, 1);
	if (FAILED(hr)) return false;

	const DirectX::Image* img = image.GetImage(0, 0, 0);
	if (!img || !img->pixels) return false;

	for (int y = 0; y < height; y++) {
		memcpy(img->pixels + y * img->rowPitch, byteRGBA.data() + y * width * 4, width * 4);
	}

	fs::path p(filePath);
	if (p.has_parent_path()) {
		std::error_code ec;
		fs::create_directories(p.parent_path(), ec);
	}

	std::wstring wPath = p.wstring();
	hr = DirectX::SaveToWICFile(
		*img,
		DirectX::WIC_FLAGS_NONE,
		DirectX::GetWICCodec(DirectX::WIC_CODEC_PNG),
		wPath.c_str()
	);
	if (FAILED(hr)) return false;

	// Immediately reload in Texture pool so Content Browser & Shader components update live
	Texture::Reload(filePath.c_str());
	return true;
}

static void ExportProceduralTextures(bool applyToSelectedObject) {
	const int resolutions[] = { 256, 512, 1024, 2048 };
	int exportRes = resolutions[std::clamp(s_exportResIdx, 0, 3)];

	std::string baseName = s_exportNameBuf;
	if (baseName.empty()) baseName = "proc_texture";

	// Strip .png extension if user typed it
	if (baseName.size() > 4 && baseName.substr(baseName.size() - 4) == ".png") {
		baseName = baseName.substr(0, baseName.size() - 4);
	}

	// Evaluate at export resolution
	if (s_editorTabMode == 1) EvaluateHlslCodeOnGPU(exportRes, exportRes, false);
	else EvaluateGraph(exportRes, exportRes, false);

	std::string albedoPath = "asset/texture/" + baseName + ".png";
	std::string normalPath = "asset/texture/" + baseName + "_normal.png";
	std::string armPath    = "asset/texture/" + baseName + "_arm.png";

	bool okAlbedo = SaveRGBABufferToPNG(s_outAlbedoRGBA, exportRes, exportRes, albedoPath);
	bool okNormal = false;
	bool okArm    = false;

	if (s_exportNormal) {
		okNormal = SaveRGBABufferToPNG(s_outNormalRGBA, exportRes, exportRes, normalPath);
	}
	if (s_exportARM) {
		okArm = SaveRGBABufferToPNG(s_outArmRGBA, exportRes, exportRes, armPath);
	}

	// Restore interactive preview buffers
	if (s_editorTabMode == 1) EvaluateHlslCodeOnGPU(s_previewResolution, s_previewResolution, true); else EvaluateGraph(s_previewResolution, s_previewResolution, true);

	if (applyToSelectedObject && Manager::GetCurrentScene()) {
		auto selected = Manager::GetCurrentScene()->GetSelectedGameObject();
		if (selected) {
			Shader* shader = selected->GetComponent<Shader>();
			if (shader) {
				if (okAlbedo) shader->SetTexturePath(albedoPath);
				if (okNormal) shader->SetNormalMapPath(normalPath);
				if (okArm)    shader->SetARMMapPath(armPath);
			}
		}
	}

	if (okAlbedo) {
		s_statusMessage = "Exported: " + albedoPath + " (" + std::to_string(exportRes) + "x" + std::to_string(exportRes) + ")";
		s_statusTimer = 5.0f;
	} else {
		s_statusMessage = "Error: Failed to export PNG to " + albedoPath;
		s_statusTimer = 5.0f;
	}
}

// =====================================================================================
// Graph Serialization (.ptex.json)
// =====================================================================================
static void SaveGraphToJson() {
	OPENFILENAMEA ofn = {};
	CHAR szFile[260] = "asset\\texture\\my_graph.ptex.json";
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = GetActiveWindow();
	ofn.lpstrFile = szFile;
	ofn.nMaxFile = sizeof(szFile);
	ofn.lpstrFilter = "Procedural Texture Graph (*.json)\0*.json\0All Files (*.*)\0*.*\0";
	ofn.nFilterIndex = 1;
	ofn.Flags = OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
	ofn.lpstrDefExt = "json";

	if (GetSaveFileNameA(&ofn) == TRUE) {
		std::ofstream ofs(ofn.lpstrFile);
		if (ofs.is_open()) {
			cereal::JSONOutputArchive ar(ofs);
			std::string exportName(s_exportNameBuf);
			ar(cereal::make_nvp("NextNodeId", s_nextNodeId),
			   cereal::make_nvp("NextLinkId", s_nextLinkId),
			   cereal::make_nvp("ExportName", exportName),
			   cereal::make_nvp("Nodes", s_nodes),
			   cereal::make_nvp("Links", s_links));
			ar(cereal::make_nvp("MappingSpace", s_mappingSpace));
			s_statusMessage = std::string("Saved graph: ") + ofn.lpstrFile;
			s_statusTimer = 4.0f;
		}
	}
}

static void LoadGraphFromJson() {
	OPENFILENAMEA ofn = {};
	CHAR szFile[260] = { 0 };
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = GetActiveWindow();
	ofn.lpstrFile = szFile;
	ofn.nMaxFile = sizeof(szFile);
	ofn.lpstrFilter = "Procedural Texture Graph (*.json)\0*.json\0All Files (*.*)\0*.*\0";
	ofn.nFilterIndex = 1;
	ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

	if (GetOpenFileNameA(&ofn) == TRUE) {
		std::ifstream ifs(ofn.lpstrFile);
		if (ifs.is_open()) {
			try {
				cereal::JSONInputArchive ar(ifs);
				std::string exportName;
				ar(cereal::make_nvp("NextNodeId", s_nextNodeId),
				   cereal::make_nvp("NextLinkId", s_nextLinkId),
				   cereal::make_nvp("ExportName", exportName),
				   cereal::make_nvp("Nodes", s_nodes),
				   cereal::make_nvp("Links", s_links));
				strcpy_s(s_exportNameBuf, exportName.c_str());
				s_mappingSpace = 0;
				try { ar(cereal::make_nvp("MappingSpace", s_mappingSpace)); } catch (...) {}
				s_graphDirty = true;
				s_statusMessage = std::string("Loaded graph: ") + ofn.lpstrFile;
				s_statusTimer = 4.0f;
			} catch (...) {
				s_statusMessage = "Error: Invalid Procedural Texture Graph JSON file.";
				s_statusTimer = 4.0f;
			}
		}
	}
}

// =====================================================================================
// Node UI Parameter Controls (drawn inside node & inspector)
// =====================================================================================
static bool DrawNodeParameters(ProcNode& node, float itemWidth) {
	bool changed = false;
	ImGui::PushItemWidth(itemWidth);


	switch (node.type) {
	case ProcNodeType::PerlinNoise:
		changed |= ImGui::SliderFloat("Scale X", &node.scaleX, 1.0f, 32.0f, "%.0f");
		changed |= ImGui::SliderFloat("Scale Y", &node.scaleY, 1.0f, 32.0f, "%.0f");
		changed |= ImGui::SliderInt("Octaves", &node.octaves, 1, 8);
		changed |= ImGui::SliderFloat("Persist", &node.persistence, 0.1f, 0.9f, "%.2f");
		changed |= ImGui::Checkbox("Ridged Noise", &node.flagA);
		changed |= ImGui::DragInt("Seed", &node.seed, 1.0f, 0, 99999);
		break;

	case ProcNodeType::Voronoi: {
		const char* modes[] = { "F1 Distance", "F2-F1 Edge/Cracks", "Cell Value" };
		changed |= ImGui::Combo("Mode", &node.mode, modes, IM_ARRAYSIZE(modes));
		changed |= ImGui::SliderFloat("Scale", &node.scaleX, 1.0f, 32.0f, "%.0f");
		changed |= ImGui::SliderFloat("Jitter", &node.paramA, 0.0f, 1.0f, "%.2f");
		changed |= ImGui::Checkbox("Invert", &node.flagA);
		changed |= ImGui::DragInt("Seed", &node.seed, 1.0f, 0, 99999);
		break;
	}

	case ProcNodeType::Checker:
		changed |= ImGui::SliderFloat("Scale X", &node.scaleX, 1.0f, 32.0f, "%.0f");
		changed |= ImGui::SliderFloat("Scale Y", &node.scaleY, 1.0f, 32.0f, "%.0f");
		changed |= ImGui::Checkbox("Grid Line Mode", &node.flagA);
		if (node.flagA) {
			changed |= ImGui::SliderFloat("Line Width", &node.paramA, 0.01f, 0.5f, "%.2f");
		}
		changed |= ImGui::ColorEdit3("Color A", node.colorA, ImGuiColorEditFlags_NoInputs);
		ImGui::SameLine();
		changed |= ImGui::ColorEdit3("Color B", node.colorB, ImGuiColorEditFlags_NoInputs);
		break;

	case ProcNodeType::Brick:
		changed |= ImGui::SliderFloat("Columns", &node.scaleX, 1.0f, 16.0f, "%.0f");
		changed |= ImGui::SliderFloat("Rows", &node.scaleY, 1.0f, 32.0f, "%.0f");
		changed |= ImGui::SliderFloat("Mortar", &node.paramA, 0.01f, 0.35f, "%.2f");
		changed |= ImGui::SliderFloat("Offset", &node.paramB, 0.0f, 1.0f, "%.2f");
		changed |= ImGui::SliderFloat("Variation", &node.paramC, 0.0f, 0.5f, "%.2f");
		changed |= ImGui::ColorEdit3("Brick", node.colorA, ImGuiColorEditFlags_NoInputs);
		ImGui::SameLine();
		changed |= ImGui::ColorEdit3("Gap", node.colorB, ImGuiColorEditFlags_NoInputs);
		break;

	case ProcNodeType::Gradient: {
		const char* modes[] = { "Linear", "Radial" };
		changed |= ImGui::Combo("Type", &node.mode, modes, IM_ARRAYSIZE(modes));
		if (node.mode == 0) {
			changed |= ImGui::SliderFloat("Angle", &node.paramA, 0.0f, 360.0f, "%.0f deg");
		}
		changed |= ImGui::SliderFloat("Repeat", &node.scaleX, 0.5f, 16.0f, "%.1f");
		changed |= ImGui::ColorEdit3("Start", node.colorA, ImGuiColorEditFlags_NoInputs);
		ImGui::SameLine();
		changed |= ImGui::ColorEdit3("End", node.colorB, ImGuiColorEditFlags_NoInputs);
		break;
	}

	case ProcNodeType::ConstantColor:
		changed |= ImGui::ColorEdit3("Color", node.colorA);
		break;

	case ProcNodeType::Blend: {
		const char* modes[] = { "Mix (Lerp)", "Multiply", "Add", "Subtract", "Screen", "Overlay", "Max", "Min" };
		changed |= ImGui::Combo("Mode", &node.mode, modes, IM_ARRAYSIZE(modes));
		changed |= ImGui::SliderFloat("Opacity", &node.paramA, 0.0f, 1.0f, "%.2f");
		break;
	}

	case ProcNodeType::ColorRamp:
		changed |= ImGui::Checkbox("3-Stop Gradient", &node.flagA);
		changed |= ImGui::ColorEdit3("##Stop0Col", node.colorA, ImGuiColorEditFlags_NoInputs);
		ImGui::SameLine();
		changed |= ImGui::SliderFloat("Stop 0", &node.paramD, 0.0f, 1.0f, "%.2f");
		if (node.flagA) {
			changed |= ImGui::ColorEdit3("##Stop1Col", node.colorB, ImGuiColorEditFlags_NoInputs);
			ImGui::SameLine();
			changed |= ImGui::SliderFloat("Stop 1", &node.paramB, 0.0f, 1.0f, "%.2f");
		}
		changed |= ImGui::ColorEdit3("##Stop2Col", node.colorC, ImGuiColorEditFlags_NoInputs);
		ImGui::SameLine();
		changed |= ImGui::SliderFloat("Stop 2", &node.paramC, 0.0f, 1.0f, "%.2f");
		break;

	case ProcNodeType::UVWarp:
		changed |= ImGui::SliderFloat("Strength", &node.paramA, 0.0f, 1.0f, "%.2f");
		break;

	case ProcNodeType::Levels:
		changed |= ImGui::SliderFloat("Bright", &node.paramA, -1.0f, 1.0f, "%.2f");
		changed |= ImGui::SliderFloat("Contrast", &node.paramB, 0.0f, 4.0f, "%.2f");
		changed |= ImGui::SliderFloat("Gamma", &node.paramC, 0.1f, 3.0f, "%.2f");
		changed |= ImGui::Checkbox("Invert", &node.flagA);
		break;

	case ProcNodeType::NormalMap:
		changed |= ImGui::SliderFloat("Strength", &node.paramA, 0.2f, 16.0f, "%.1f");
		changed |= ImGui::Checkbox("Invert Y", &node.flagA);
		break;

	case ProcNodeType::Output:
		changed |= ImGui::SliderFloat("Roughness", &node.paramA, 0.0f, 1.0f, "%.2f");
		changed |= ImGui::SliderFloat("Metallic", &node.paramB, 0.0f, 1.0f, "%.2f");
		changed |= ImGui::SliderFloat("Base AO", &node.paramC, 0.0f, 1.0f, "%.2f");
		break;

	case ProcNodeType::CustomHLSL: {
		EnsureDefaultCustomHlslFile();
		std::vector<std::string> hlslFiles;
		std::error_code ec;
		if (fs::exists("shader", ec)) {
			for (const auto& entry : fs::recursive_directory_iterator("shader", ec)) {
				if (entry.is_regular_file() && entry.path().extension() == ".hlsl") {
					hlslFiles.push_back(entry.path().generic_string());
				}
			}
		}
		std::sort(hlslFiles.begin(), hlslFiles.end());

		std::string currentLabel = node.customHlslFile.empty() ? "(Select .hlsl)" : fs::path(node.customHlslFile).filename().string();
		ImGui::SetNextItemWidth(itemWidth + 62.0f);
		if (ImGui::BeginCombo("##CustomHlslCombo", currentLabel.c_str())) {
			for (const auto& f : hlslFiles) {
				bool isSel = (node.customHlslFile == f);
				if (ImGui::Selectable(f.c_str(), isSel)) {
					node.customHlslFile = f;
					changed = true;
				}
				if (isSel) ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
		if (ImGui::BeginDragDropTarget()) {
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DND_ASSET_PATH")) {
				std::string droppedPath(static_cast<const char*>(payload->Data));
				std::replace(droppedPath.begin(), droppedPath.end(), '\\', '/');
				if (fs::path(droppedPath).extension() == ".hlsl") {
					node.customHlslFile = droppedPath;
					changed = true;
				}
			}
			ImGui::EndDragDropTarget();
		}

		if (ImGui::SmallButton("Edit in HLSL Tab")) {
			std::string content = LoadHlslFileContent(node.customHlslFile);
			if (!content.empty()) {
				s_hlslCodeText = content;
				std::string stem = fs::path(node.customHlslFile).stem().string();
				strcpy_s(s_hlslFileNameBuf, stem.c_str());
				s_editorTabMode = 1;
				s_graphDirty = true;
			}
		}
		ImGui::SameLine();
		if (ImGui::SmallButton("Ext Editor") && !node.customHlslFile.empty()) {
			Manager::OpenInTextEditor(node.customHlslFile);
		}
		ImGui::SameLine();
		if (ImGui::SmallButton("Reload")) {
			s_computePsoCache.clear();
			changed = true;
		}

		changed |= ImGui::SliderFloat("Scale X", &node.scaleX, 1.0f, 32.0f, "%.0f");
		changed |= ImGui::SliderFloat("Scale Y", &node.scaleY, 1.0f, 32.0f, "%.0f");
		changed |= ImGui::SliderFloat("Param A", &node.paramA, 0.0f, 1.0f, "%.2f");
		changed |= ImGui::SliderFloat("Param B", &node.paramB, 0.0f, 1.0f, "%.2f");
		changed |= ImGui::SliderFloat("Param C", &node.paramC, 0.0f, 1.0f, "%.2f");
		changed |= ImGui::SliderFloat("Param D", &node.paramD, 0.0f, 1.0f, "%.2f");
		changed |= ImGui::DragInt("Seed", &node.seed, 1.0f, 0, 99999);
		changed |= ImGui::ColorEdit3("Col A", node.colorA, ImGuiColorEditFlags_NoInputs);
		ImGui::SameLine();
		changed |= ImGui::ColorEdit3("Col B", node.colorB, ImGuiColorEditFlags_NoInputs);
		break;
	}
		break;
	}

	ImGui::PopItemWidth();
	return changed;
}

} // anonymous namespace

// =====================================================================================
// Public Interface
// =====================================================================================
void ProceduralTextureEditor::Initialize() {
	if (s_initialized) return;
	s_initialized = true;
	EnsureDefaultCustomHlslFile();
	LoadPresetOutputOnly();
}

void ProceduralTextureEditor::RenderUI(bool* p_open) {
	if (!s_initialized) {
		Initialize();
	}

	if (s_graphDirty) {
		if (s_editorTabMode == 1) EvaluateHlslCodeOnGPU(s_previewResolution, s_previewResolution, true); else EvaluateGraph(s_previewResolution, s_previewResolution, true);
		s_graphDirty = false;
	}

	ImGui::SetNextWindowSize(ImVec2(1100.0f, 680.0f), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Procedural Texture Editor", p_open, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
		ImGui::End();
		return;
	}

	// =================================================================================
	// Top Toolbar
	// =================================================================================
	if (ImGui::Button("+ Add Node")) {
		ImGui::OpenPopup("AddProcNodePopup");
	}
	if (ImGui::BeginPopup("AddProcNodePopup")) {
		ImVec2 spawnPos = ImVec2(-s_canvasScroll.x + 220.0f, -s_canvasScroll.y + 160.0f);
		ImGui::TextDisabled("Generators");
		if (ImGui::MenuItem("Perlin / FBM Noise")) AddNode(ProcNodeType::PerlinNoise, spawnPos);
		if (ImGui::MenuItem("Voronoi / Cellular")) AddNode(ProcNodeType::Voronoi, spawnPos);
		if (ImGui::MenuItem("Checker / Grid"))     AddNode(ProcNodeType::Checker, spawnPos);
		if (ImGui::MenuItem("Brick Pattern"))      AddNode(ProcNodeType::Brick, spawnPos);
		if (ImGui::MenuItem("Gradient"))           AddNode(ProcNodeType::Gradient, spawnPos);
		if (ImGui::MenuItem("Constant Color"))     AddNode(ProcNodeType::ConstantColor, spawnPos);
		ImGui::Separator();
		ImGui::TextDisabled("Filters & Operators");
		if (ImGui::MenuItem("Blend (Mix / Multiply / Add...)")) AddNode(ProcNodeType::Blend, spawnPos);
		if (ImGui::MenuItem("Color Ramp (Gradient Map)"))       AddNode(ProcNodeType::ColorRamp, spawnPos);
		if (ImGui::MenuItem("UV Warp (Domain Warp)"))           AddNode(ProcNodeType::UVWarp, spawnPos);
		if (ImGui::MenuItem("Levels / Math"))                   AddNode(ProcNodeType::Levels, spawnPos);
		if (ImGui::MenuItem("Height to Normal Map"))            AddNode(ProcNodeType::NormalMap, spawnPos);
		ImGui::Separator();
		ImGui::TextDisabled("HLSL / GPU Custom");
		if (ImGui::MenuItem("Custom HLSL (.hlsl)"))             AddNode(ProcNodeType::CustomHLSL, spawnPos);
		ImGui::EndPopup();
	}

	ImGui::SameLine();
	if (ImGui::Button("Presets...")) {
		ImGui::OpenPopup("ProcPresetsPopup");
	}
	if (ImGui::BeginPopup("ProcPresetsPopup")) {
		if (ImGui::MenuItem("Default / Empty (Output Node Only)"))                 LoadPresetOutputOnly();
		ImGui::Separator();
		if (ImGui::MenuItem("Weathered Brick Wall (Albedo + Normal + Roughness)")) LoadPresetBrickWall();
		if (ImGui::MenuItem("Veined Polished Marble (Albedo + Normal)"))           LoadPresetMarble();
		if (ImGui::MenuItem("Cracked Lava / Basalt Rock (Albedo + Normal + AO)"))  LoadPresetCrackedLava();
		if (ImGui::MenuItem("Rusted Metal Plate (Albedo + Normal + ARM)"))         LoadPresetRustedMetal();
		ImGui::Separator();
		ImGui::TextDisabled("3D Spherical Presets (SkyDome / Sphere)");
		if (ImGui::MenuItem("SkyDome Cloudy Sky (3D Spherical)"))                  LoadPresetSkyDomeClouds();
		if (ImGui::MenuItem("Planet / Asteroid Surface (3D Spherical + PBR)"))     LoadPresetPlanetSurface();
		ImGui::EndPopup();
	}

	ImGui::SameLine();
	if (ImGui::Button("Save Graph (.json)")) {
		SaveGraphToJson();
	}
	ImGui::SameLine();
	if (ImGui::Button("Load Graph (.json)")) {
		LoadGraphFromJson();
	}
	ImGui::SameLine();
	if (ImGui::Button("Reset View")) {
		s_canvasScroll = ImVec2(0.0f, 0.0f);
	}
	ImGui::SameLine(0.0f, 12.0f);
	{
		bool isNodeMode = (s_editorTabMode == 0);
		if (isNodeMode) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.22f, 0.48f, 0.78f, 1.0f));
		if (ImGui::Button("Node Graph")) {
			s_editorTabMode = 0;
			s_graphDirty = true;
		}
		if (isNodeMode) ImGui::PopStyleColor();

		ImGui::SameLine();
		bool isHlslMode = (s_editorTabMode == 1);
		if (isHlslMode) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.72f, 0.44f, 0.16f, 1.0f));
		if (ImGui::Button("HLSL Code (DX12 CS)")) {
			if (s_hlslCodeText.empty()) {
				s_hlslCodeText = GenerateHLSLFromGraph();
			}
			s_editorTabMode = 1;
			s_graphDirty = true;
		}
		if (isHlslMode) ImGui::PopStyleColor();

		ImGui::SameLine();
		if (ImGui::Button("Nodes -> HLSL")) {
			s_hlslCodeText = GenerateHLSLFromGraph();
			s_editorTabMode = 1;
			s_graphDirty = true;
			s_statusMessage = "Generated HLSL code from current Node Graph.";
			s_statusTimer = 4.0f;
		}
	}
	if (false) {
	}

	ImGui::SameLine(0.0f, 16.0f);
	ImGui::TextUnformatted("Mapping:");
	ImGui::SameLine();
	ImGui::SetNextItemWidth(210.0f);
	const char* toolbarMappingModes[] = {
		"2D Planar (Tileable)",
		"3D Spherical (SkyDome / Sphere)",
		"3D Box / Cube-Sphere"
	};
	if (ImGui::Combo("##ToolbarMappingSpace", &s_mappingSpace, toolbarMappingModes, IM_ARRAYSIZE(toolbarMappingModes))) {
		s_graphDirty = true;
	}

	if (s_statusTimer > 0.0f) {
		s_statusTimer -= ImGui::GetIO().DeltaTime;
		ImGui::SameLine(0.0f, 16.0f);
		ImGui::TextColored(ImVec4(0.35f, 0.90f, 0.50f, 1.0f), "%s", s_statusMessage.c_str());
	}

	ImGui::Separator();

	// =================================================================================
	// 2-Pane Split: Left = Node Graph Canvas, Right = Preview & Export Panel
	// =================================================================================
	float rightPaneWidth = 300.0f;
	ImVec2 avail = ImGui::GetContentRegionAvail();
	float canvasWidth = std::max(200.0f, avail.x - rightPaneWidth - 8.0f);

	// ---------------------------------------------------------------------------------
	// LEFT PANE: Node Graph Canvas
	// ---------------------------------------------------------------------------------
	ImGui::BeginChild("NodeGraphCanvas", ImVec2(canvasWidth, avail.y), true,
		ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollWithMouse);
	{
		if (s_editorTabMode == 1) {
			EnsureDefaultCustomHlslFile();
			ImGui::TextColored(ImVec4(0.95f, 0.75f, 0.35f, 1.0f), "DirectX 12 Compute Shader (cs_5_0) HLSL Editor");
			ImGui::SameLine(0.0f, 16.0f);
			if (ImGui::SmallButton("Compile & Run on GPU (F5)")) {
				EvaluateHlslCodeOnGPU(s_previewResolution, s_previewResolution, true);
			}

			ImGui::Spacing();
			ImGui::TextUnformatted("HLSL File Name (shader/):");
			ImGui::SameLine();
			ImGui::SetNextItemWidth(180.0f);
			ImGui::InputText("##HlslFileName", s_hlslFileNameBuf, IM_ARRAYSIZE(s_hlslFileNameBuf));
			ImGui::SameLine();
			if (ImGui::Button("Save to shader/")) {
				std::string savedPath;
				if (SaveHlslToShaderFolder(s_hlslFileNameBuf, s_hlslCodeText, savedPath)) {
					s_computePsoCache.clear();
					s_statusMessage = "Saved HLSL: " + savedPath;
					s_statusTimer = 4.0f;
				} else {
					s_statusMessage = "Error: Failed to save HLSL to shader/";
					s_statusTimer = 4.0f;
				}
			}
			ImGui::SameLine();
			if (ImGui::Button("Load from shader/...")) {
				ImGui::OpenPopup("LoadHlslFromShaderPopup");
			}
			if (ImGui::BeginPopup("LoadHlslFromShaderPopup")) {
				std::vector<std::string> hlslFiles;
				std::error_code ec;
				if (fs::exists("shader", ec)) {
					for (const auto& entry : fs::recursive_directory_iterator("shader", ec)) {
						if (entry.is_regular_file() && entry.path().extension() == ".hlsl") {
							hlslFiles.push_back(entry.path().generic_string());
						}
					}
				}
				std::sort(hlslFiles.begin(), hlslFiles.end());
				for (const auto& f : hlslFiles) {
					if (ImGui::MenuItem(f.c_str())) {
						std::string loaded = LoadHlslFileContent(f);
						if (!loaded.empty()) {
							s_hlslCodeText = loaded;
							std::string stem = fs::path(f).stem().string();
							strcpy_s(s_hlslFileNameBuf, stem.c_str());
							EvaluateHlslCodeOnGPU(s_previewResolution, s_previewResolution, true);
						}
					}
				}
				ImGui::EndPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("Open in Ext Editor")) {
				std::string savedPath;
				if (SaveHlslToShaderFolder(s_hlslFileNameBuf, s_hlslCodeText, savedPath)) {
					Manager::OpenInTextEditor(savedPath);
				}
			}
			ImGui::SameLine();
			if (ImGui::Button("Use as Node in Graph")) {
				std::string savedPath;
				if (SaveHlslToShaderFolder(s_hlslFileNameBuf, s_hlslCodeText, savedPath)) {
					ImVec2 spawnPos = ImVec2(-s_canvasScroll.x + 220.0f, -s_canvasScroll.y + 160.0f);
					int nid = AddNode(ProcNodeType::CustomHLSL, spawnPos).id;
					ProcNode* n = FindNode(nid);
					if (n) n->customHlslFile = savedPath;
					s_editorTabMode = 0;
					s_graphDirty = true;
					s_statusMessage = "Created Custom HLSL node from " + savedPath;
					s_statusTimer = 4.0f;
				}
			}

			ImVec2 editorAvail = ImGui::GetContentRegionAvail();
			float logHeight = 68.0f;
			float codeHeight = std::max(120.0f, editorAvail.y - logHeight - 8.0f);

			auto resizeCallback = [](ImGuiInputTextCallbackData* data) -> int {
				if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
					std::string* str = static_cast<std::string*>(data->UserData);
					str->resize(data->BufTextLen);
					data->Buf = str->data();
				}
				return 0;
			};

			if (s_hlslCodeText.empty()) {
				s_hlslCodeText = GenerateHLSLFromGraph();
			}
			ImGui::InputTextMultiline(
				"##HlslSourceCodeEditor",
				s_hlslCodeText.data(),
				s_hlslCodeText.capacity() + 1,
				ImVec2(-1.0f, codeHeight),
				ImGuiInputTextFlags_AllowTabInput | ImGuiInputTextFlags_CallbackResize,
				resizeCallback,
				&s_hlslCodeText
			);
			if (ImGui::BeginDragDropTarget()) {
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DND_ASSET_PATH")) {
					std::string droppedPath(static_cast<const char*>(payload->Data));
					std::replace(droppedPath.begin(), droppedPath.end(), '\\', '/');
					if (fs::path(droppedPath).extension() == ".hlsl") {
						std::string loaded = LoadHlslFileContent(droppedPath);
						if (!loaded.empty()) {
							s_hlslCodeText = loaded;
							std::string stem = fs::path(droppedPath).stem().string();
							strcpy_s(s_hlslFileNameBuf, stem.c_str());
							EvaluateHlslCodeOnGPU(s_previewResolution, s_previewResolution, true);
						}
					}
				}
				ImGui::EndDragDropTarget();
			}

			ImGui::BeginChild("HlslCompilerLogBox", ImVec2(-1.0f, logHeight), true);
			ImVec4 logCol = s_hlslLastCompileSuccess ? ImVec4(0.40f, 0.92f, 0.52f, 1.0f) : ImVec4(1.0f, 0.42f, 0.38f, 1.0f);
			ImGui::TextColored(logCol, "%s", s_hlslCompileLog.empty() ? "Ready. Click 'Compile & Run on GPU' to dispatch Compute Shader (cs_5_0)." : s_hlslCompileLog.c_str());
			ImGui::EndChild();
		} else {
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		ImVec2 canvasPos = ImGui::GetCursorScreenPos();
		ImVec2 canvasSize = ImGui::GetContentRegionAvail();
		ImVec2 origin = ImVec2(canvasPos.x + s_canvasScroll.x, canvasPos.y + s_canvasScroll.y);

		// Background & Grid
		drawList->AddRectFilled(canvasPos, ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), IM_COL32(22, 24, 29, 255));
		const float gridStep = 32.0f;
		for (float x = std::fmod(s_canvasScroll.x, gridStep); x < canvasSize.x; x += gridStep) {
			drawList->AddLine(ImVec2(canvasPos.x + x, canvasPos.y), ImVec2(canvasPos.x + x, canvasPos.y + canvasSize.y), IM_COL32(255, 255, 255, 14));
		}
		for (float y = std::fmod(s_canvasScroll.y, gridStep); y < canvasSize.y; y += gridStep) {
			drawList->AddLine(ImVec2(canvasPos.x, canvasPos.y + y), ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + y), IM_COL32(255, 255, 255, 14));
		}

		// Record pin screen positions during node layout
		std::unordered_map<uint64_t, ImVec2> inputPinPos;
		std::unordered_map<uint64_t, ImVec2> outputPinPos;
		auto makePinKey = [](int nodeId, int pinIdx) -> uint64_t {
			return (static_cast<uint64_t>(static_cast<uint32_t>(nodeId)) << 32) | static_cast<uint32_t>(pinIdx);
		};

		drawList->ChannelsSplit(2); // Channel 0: links & node backgrounds, Channel 1: node widgets & pins

		const float nodeWidth = 210.0f;
		int hoveredPinNodeId = -1;
		int hoveredPinIdx = -1;
		bool hoveredPinIsOutput = false;
		int nodeToDelete = -1;

		for (size_t idx = 0; idx < s_nodes.size(); idx++) {
			ProcNode& node = s_nodes[idx];
			NodeMeta meta = GetNodeMeta(node.type);
			ImGui::PushID(node.id);

			ImVec2 nodeScreenPos = ImVec2(origin.x + node.pos.x, origin.y + node.pos.y);

			drawList->ChannelsSetCurrent(1);

			// Header drag button
			ImGui::SetCursorScreenPos(nodeScreenPos);
			ImGui::InvisibleButton("##HeaderDrag", ImVec2(nodeWidth, 26.0f));
			if (ImGui::IsItemActive()) {
				if (s_selectedNodeId != node.id) {
					s_selectedNodeId = node.id;
					s_preview3DDirty = true;
				}
				if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
					node.pos.x += ImGui::GetIO().MouseDelta.x;
					node.pos.y += ImGui::GetIO().MouseDelta.y;
				}
			}
			if (ImGui::IsItemClicked(ImGuiMouseButton_Right) && node.type != ProcNodeType::Output) {
				ImGui::OpenPopup("NodeContextMenu");
			}
			if (ImGui::BeginPopup("NodeContextMenu")) {
				if (ImGui::MenuItem("Delete Node")) {
					nodeToDelete = node.id;
				}
				ImGui::EndPopup();
			}

			// Draw title text
			drawList->AddText(ImVec2(nodeScreenPos.x + 10.0f, nodeScreenPos.y + 5.0f), IM_COL32(245, 245, 250, 255), meta.title);

			// Draw Pins & Mini-Preview row
			float contentTopY = nodeScreenPos.y + 32.0f;
			size_t maxPins = std::max(meta.inputPins.size(), meta.outputPins.size());
			float pinsHeight = static_cast<float>(maxPins) * 22.0f;

			// Input pins (left edge)
			for (size_t p = 0; p < meta.inputPins.size(); p++) {
				ImVec2 pPos = ImVec2(nodeScreenPos.x, contentTopY + p * 22.0f + 10.0f);
				inputPinPos[makePinKey(node.id, static_cast<int>(p))] = pPos;

				ImGui::SetCursorScreenPos(ImVec2(pPos.x - 8.0f, pPos.y - 8.0f));
				ImGui::PushID(static_cast<int>(p));
				ImGui::InvisibleButton("##InPin", ImVec2(16.0f, 16.0f));
				bool pinHovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
				if (pinHovered) {
					hoveredPinNodeId = node.id;
					hoveredPinIdx = static_cast<int>(p);
					hoveredPinIsOutput = false;
				}
				if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
					// Right-click input pin to disconnect
					int targetId = node.id;
					int targetPin = static_cast<int>(p);
					s_links.erase(std::remove_if(s_links.begin(), s_links.end(), [&](const ProcNodeLink& l) {
						return l.toNodeId == targetId && l.toPinIdx == targetPin;
					}), s_links.end());
					s_graphDirty = true;
				}
				if (ImGui::IsItemActivated()) {
					s_draggingPinNodeId = node.id;
					s_draggingPinIdx = static_cast<int>(p);
					s_draggingPinIsOutput = false;
				}
				ImGui::PopID();

				drawList->AddCircleFilled(pPos, pinHovered ? 6.5f : 5.0f, IM_COL32(110, 200, 255, 255));
				drawList->AddCircle(pPos, pinHovered ? 6.5f : 5.0f, IM_COL32(25, 30, 35, 255), 0, 1.5f);
				drawList->AddText(ImVec2(pPos.x + 10.0f, pPos.y - 7.0f), IM_COL32(210, 215, 225, 255), meta.inputPins[p]);
			}

			// Output pins (right edge)
			for (size_t p = 0; p < meta.outputPins.size(); p++) {
				ImVec2 pPos = ImVec2(nodeScreenPos.x + nodeWidth, contentTopY + p * 22.0f + 10.0f);
				outputPinPos[makePinKey(node.id, static_cast<int>(p))] = pPos;

				ImGui::SetCursorScreenPos(ImVec2(pPos.x - 8.0f, pPos.y - 8.0f));
				ImGui::PushID(100 + static_cast<int>(p));
				ImGui::InvisibleButton("##OutPin", ImVec2(16.0f, 16.0f));
				bool pinHovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
				if (pinHovered) {
					hoveredPinNodeId = node.id;
					hoveredPinIdx = static_cast<int>(p);
					hoveredPinIsOutput = true;
				}
				if (ImGui::IsItemActivated()) {
					s_draggingPinNodeId = node.id;
					s_draggingPinIdx = static_cast<int>(p);
					s_draggingPinIsOutput = true;
				}
				ImGui::PopID();

				ImVec2 textSize = ImGui::CalcTextSize(meta.outputPins[p]);
				drawList->AddCircleFilled(pPos, pinHovered ? 6.5f : 5.0f, IM_COL32(255, 195, 85, 255));
				drawList->AddCircle(pPos, pinHovered ? 6.5f : 5.0f, IM_COL32(25, 30, 35, 255), 0, 1.5f);
				drawList->AddText(ImVec2(pPos.x - 10.0f - textSize.x, pPos.y - 7.0f), IM_COL32(210, 215, 225, 255), meta.outputPins[p]);
			}

			// Mini thumbnail preview centered inside node
			float bodyY = contentTopY + std::max(22.0f, pinsHeight) + 4.0f;
			if (node.previewTexture) {
				void* srv = Renderer::GetTextureSRV(node.previewTexture);
				if (srv) {
					ImGui::SetCursorScreenPos(ImVec2(nodeScreenPos.x + (nodeWidth - 56.0f) * 0.5f, contentTopY));
					ImGui::Image(srv, ImVec2(56.0f, 56.0f));
					bodyY = std::max(bodyY, contentTopY + 62.0f);
				}
			}

			// Node inline parameters
			ImGui::SetCursorScreenPos(ImVec2(nodeScreenPos.x + 10.0f, bodyY));
			ImGui::BeginGroup();
			if (DrawNodeParameters(node, 118.0f)) {
				if (s_selectedNodeId != node.id) {
					s_selectedNodeId = node.id;
					s_preview3DDirty = true;
				}
				s_graphDirty = true;
			}
			ImGui::EndGroup();

			float nodeBottomY = ImGui::GetItemRectMax().y + 8.0f;
			ImVec2 nodeMax = ImVec2(nodeScreenPos.x + nodeWidth, nodeBottomY);

			// Draw Node Box in Channel 0
			drawList->ChannelsSetCurrent(0);
			bool isSelected = (s_selectedNodeId == node.id);
			drawList->AddRectFilled(nodeScreenPos, nodeMax, IM_COL32(36, 39, 46, 240), 6.0f);
			drawList->AddRectFilled(nodeScreenPos, ImVec2(nodeMax.x, nodeScreenPos.y + 26.0f), meta.headerColor, 6.0f, ImDrawFlags_RoundCornersTop);
			drawList->AddRect(nodeScreenPos, nodeMax,
				isSelected ? IM_COL32(95, 175, 255, 255) : IM_COL32(70, 75, 88, 200),
				6.0f, 0, isSelected ? 2.2f : 1.0f);

			ImGui::PopID();
		}

		// Handle node deletion
		if (nodeToDelete != -1) {
			s_nodes.erase(std::remove_if(s_nodes.begin(), s_nodes.end(), [&](const ProcNode& n) {
				return n.id == nodeToDelete;
			}), s_nodes.end());
			s_links.erase(std::remove_if(s_links.begin(), s_links.end(), [&](const ProcNodeLink& l) {
				return l.fromNodeId == nodeToDelete || l.toNodeId == nodeToDelete;
			}), s_links.end());
			if (s_selectedNodeId == nodeToDelete) s_selectedNodeId = -1;
			s_graphDirty = true;
		}

		// Draw Bezier Links in Channel 0
		drawList->ChannelsSetCurrent(0);
		for (const auto& link : s_links) {
			auto itOut = outputPinPos.find(makePinKey(link.fromNodeId, link.fromPinIdx));
			auto itIn  = inputPinPos.find(makePinKey(link.toNodeId, link.toPinIdx));
			if (itOut != outputPinPos.end() && itIn != inputPinPos.end()) {
				ImVec2 p1 = itOut->second;
				ImVec2 p2 = itIn->second;
				float dist = std::max(40.0f, std::fabs(p2.x - p1.x) * 0.5f);
				drawList->AddBezierCubic(
					p1,
					ImVec2(p1.x + dist, p1.y),
					ImVec2(p2.x - dist, p2.y),
					p2,
					IM_COL32(235, 185, 80, 230),
					3.0f
				);
			}
		}

		// Active Dragging Link Wire
		if (s_draggingPinNodeId != -1) {
			ImVec2 p1 = ImGui::GetIO().MousePos;
			if (s_draggingPinIsOutput) {
				auto it = outputPinPos.find(makePinKey(s_draggingPinNodeId, s_draggingPinIdx));
				if (it != outputPinPos.end()) p1 = it->second;
			} else {
				auto it = inputPinPos.find(makePinKey(s_draggingPinNodeId, s_draggingPinIdx));
				if (it != inputPinPos.end()) p1 = it->second;
			}
			ImVec2 p2 = ImGui::GetIO().MousePos;
			drawList->AddBezierCubic(
				p1,
				ImVec2(p1.x + (s_draggingPinIsOutput ? 50.0f : -50.0f), p1.y),
				ImVec2(p2.x + (s_draggingPinIsOutput ? -50.0f : 50.0f), p2.y),
				p2,
				IM_COL32(120, 210, 255, 240),
				2.5f
			);

			if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
				if (hoveredPinNodeId != -1 && hoveredPinNodeId != s_draggingPinNodeId && hoveredPinIsOutput != s_draggingPinIsOutput) {
					if (s_draggingPinIsOutput) {
						AddLink(s_draggingPinNodeId, s_draggingPinIdx, hoveredPinNodeId, hoveredPinIdx);
					} else {
						AddLink(hoveredPinNodeId, hoveredPinIdx, s_draggingPinNodeId, s_draggingPinIdx);
					}
				}
				s_draggingPinNodeId = -1;
				s_draggingPinIdx = -1;
			}
		}

		drawList->ChannelsMerge();

		// Canvas Panning (Middle mouse drag or Right mouse drag) & Context Menu
		if (ImGui::IsWindowHovered() && !ImGui::IsAnyItemActive()) {
			if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.0f)) {
				s_canvasScroll.x += ImGui::GetIO().MouseDelta.x;
				s_canvasScroll.y += ImGui::GetIO().MouseDelta.y;
			}
			if (ImGui::IsMouseReleased(ImGuiMouseButton_Right) && !ImGui::IsMouseDragPastThreshold(ImGuiMouseButton_Right)) {
				ImGui::OpenPopup("CanvasContextMenu");
			}
		}

		if (ImGui::BeginPopup("CanvasContextMenu")) {
			ImVec2 clickCanvasPos = ImVec2(ImGui::GetMousePosOnOpeningCurrentPopup().x - origin.x,
			                               ImGui::GetMousePosOnOpeningCurrentPopup().y - origin.y);
			ImGui::TextDisabled("Add Node Here");
			ImGui::Separator();
			if (ImGui::MenuItem("Perlin / FBM Noise")) AddNode(ProcNodeType::PerlinNoise, clickCanvasPos);
			if (ImGui::MenuItem("Voronoi / Cellular")) AddNode(ProcNodeType::Voronoi, clickCanvasPos);
			if (ImGui::MenuItem("Checker / Grid"))     AddNode(ProcNodeType::Checker, clickCanvasPos);
			if (ImGui::MenuItem("Brick Pattern"))      AddNode(ProcNodeType::Brick, clickCanvasPos);
			if (ImGui::MenuItem("Gradient"))           AddNode(ProcNodeType::Gradient, clickCanvasPos);
			if (ImGui::MenuItem("Constant Color"))     AddNode(ProcNodeType::ConstantColor, clickCanvasPos);
			ImGui::Separator();
			if (ImGui::MenuItem("Blend"))              AddNode(ProcNodeType::Blend, clickCanvasPos);
			if (ImGui::MenuItem("Color Ramp"))         AddNode(ProcNodeType::ColorRamp, clickCanvasPos);
			if (ImGui::MenuItem("UV Warp"))            AddNode(ProcNodeType::UVWarp, clickCanvasPos);
			if (ImGui::MenuItem("Levels / Math"))      AddNode(ProcNodeType::Levels, clickCanvasPos);
			if (ImGui::MenuItem("Height to Normal"))   AddNode(ProcNodeType::NormalMap, clickCanvasPos);
			ImGui::Separator();
			if (ImGui::MenuItem("Custom HLSL (.hlsl)")) AddNode(ProcNodeType::CustomHLSL, clickCanvasPos);
			ImGui::EndPopup();
		}

		// Overlay hint at bottom-left of canvas
		drawList->AddText(
			ImVec2(canvasPos.x + 10.0f, canvasPos.y + canvasSize.y - 22.0f),
			IM_COL32(160, 165, 175, 180),
			"Middle-Drag: Pan | Right-Click Canvas: Add Node | Right-Click Input Pin: Disconnect"); } if (false) ImGui::TextUnformatted(""
		);
	}
	ImGui::EndChild();

	ImGui::SameLine();

	// ---------------------------------------------------------------------------------
	// RIGHT PANE: Live Preview & PNG Export Panel
	// ---------------------------------------------------------------------------------
	ImGui::BeginChild("PreviewAndExportPane", ImVec2(rightPaneWidth, avail.y), true);
	{
		ImGui::TextUnformatted("Mapping Space:");
		const char* mappingNames[] = {
			"2D Planar (Tileable)",
			"3D Spherical (SkyDome / Sphere)",
			"3D Box / Cube-Sphere"
		};
		ImGui::SetNextItemWidth(-1.0f);
		if (ImGui::Combo("##MappingSpacePane", &s_mappingSpace, mappingNames, IM_ARRAYSIZE(mappingNames))) {
			s_graphDirty = true;
		}

		ImGui::Spacing();
		ImGui::TextUnformatted("Preview View & Channel:");
		const char* shapeNames[] = {
			"2D Flat",
			"3D Sphere",
			"3D Cube",
			"3D SkyDome (Inside)"
		};
		ImGui::SetNextItemWidth(-1.0f);
		if (ImGui::Combo("##PreviewShapeMode", &s_previewShapeMode, shapeNames, IM_ARRAYSIZE(shapeNames))) {
			s_preview3DDirty = true;
		}
		const char* channelNames[] = { "Output Albedo", "Output Normal", "Output ARM", "Selected Node" };
		ImGui::SetNextItemWidth(-1.0f);
		if (ImGui::Combo("##PreviewChannel", &s_previewChannel, channelNames, IM_ARRAYSIZE(channelNames))) {
			s_preview3DDirty = true;
		}

		if (s_previewShapeMode == 0) {
			ImGui::Checkbox("2x2 Tiling Preview", &s_tilePreview2x2);
		} else {
			if (s_previewShapeMode != 3 && s_previewChannel == 0) {
				if (ImGui::Checkbox("PBR Lighting", &s_previewPBRShading)) {
					s_preview3DDirty = true;
				}
				ImGui::SameLine();
			}
			if (ImGui::SmallButton("Reset 3D")) {
				s_previewYaw = 0.45f;
				s_previewPitch = 0.25f;
				s_lightYaw = -0.65f;
				s_lightPitch = 0.55f;
				s_preview3DDirty = true;
			}
		}

		const TEXTURE* previewTex = s_outAlbedoTex;
		if (s_previewChannel == 1) previewTex = s_outNormalTex;
		else if (s_previewChannel == 2) previewTex = s_outArmTex;
		else if (s_previewChannel == 3) {
			ProcNode* sel = FindNode(s_selectedNodeId);
			if (sel && sel->previewTexture) previewTex = sel->previewTexture;
		}

		float previewSize = rightPaneWidth - 20.0f;
		if (s_previewShapeMode != 0) {
			if (s_preview3DDirty || !s_preview3DTex) {
				Render3DPreviewImage();
			}
			if (s_preview3DTex) {
				void* srv3d = Renderer::GetTextureSRV(s_preview3DTex);
				if (srv3d) {
					ImVec2 p0 = ImGui::GetCursorScreenPos();
					ImGui::InvisibleButton("##3DPreviewCanvas", ImVec2(previewSize, previewSize),
						ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
					if (ImGui::IsItemActive()) {
						ImVec2 delta = ImGui::GetIO().MouseDelta;
						if (ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f)) {
							s_previewYaw   += delta.x * 0.012f;
							s_previewPitch  = std::clamp(s_previewPitch + delta.y * 0.012f, -1.45f, 1.45f);
							s_preview3DDirty = true;
						}
						if (ImGui::IsMouseDragging(ImGuiMouseButton_Right, 0.0f)) {
							s_lightYaw   += delta.x * 0.014f;
							s_lightPitch  = std::clamp(s_lightPitch - delta.y * 0.014f, -1.45f, 1.45f);
							s_preview3DDirty = true;
						}
					}
					ImDrawList* dl = ImGui::GetWindowDrawList();
					dl->AddImage(srv3d, p0, ImVec2(p0.x + previewSize, p0.y + previewSize));
					dl->AddRect(p0, ImVec2(p0.x + previewSize, p0.y + previewSize), IM_COL32(80, 90, 110, 180));
					ImGui::TextDisabled("L-Drag: Rotate 3D | R-Drag: Move Light");
				}
			}
		} else
		if (previewTex) {
			void* srv = Renderer::GetTextureSRV(previewTex);
			if (srv) {
				if (!s_tilePreview2x2) {
					ImGui::Image(srv, ImVec2(previewSize, previewSize));
				} else {
					float half = (previewSize - 2.0f) * 0.5f;
					ImVec2 p0 = ImGui::GetCursorScreenPos();
					ImDrawList* dl = ImGui::GetWindowDrawList();
					dl->AddImage(srv, p0, ImVec2(p0.x + half, p0.y + half));
					dl->AddImage(srv, ImVec2(p0.x + half, p0.y), ImVec2(p0.x + half * 2.0f, p0.y + half));
					dl->AddImage(srv, ImVec2(p0.x, p0.y + half), ImVec2(p0.x + half, p0.y + half * 2.0f));
					dl->AddImage(srv, ImVec2(p0.x + half, p0.y + half), ImVec2(p0.x + half * 2.0f, p0.y + half * 2.0f));
					ImGui::Dummy(ImVec2(previewSize, previewSize));
				}
			}
		}

		ImGui::Separator();
		ImGui::TextUnformatted("Export to PNG (asset/texture/)");

		ImGui::TextUnformatted("Texture Name:");
		ImGui::SetNextItemWidth(-1.0f);
		ImGui::InputText("##ExportName", s_exportNameBuf, IM_ARRAYSIZE(s_exportNameBuf));

		const char* resItems[] = { "256 x 256", "512 x 512", "1024 x 1024", "2048 x 2048" };
		ImGui::TextUnformatted("Export Resolution:");
		ImGui::SetNextItemWidth(-1.0f);
		ImGui::Combo("##ExportRes", &s_exportResIdx, resItems, IM_ARRAYSIZE(resItems));

		ImGui::Checkbox("Also Export _normal.png", &s_exportNormal);
		ImGui::Checkbox("Also Export _arm.png", &s_exportARM);

		ImGui::Spacing();
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.55f, 0.34f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.24f, 0.68f, 0.42f, 1.0f));
		if (ImGui::Button("Export PNG to asset/texture/", ImVec2(-1.0f, 32.0f))) {
			ExportProceduralTextures(false);
		}
		ImGui::PopStyleColor(2);

		// Quick Apply to Selected GameObject's Shader component
		auto selectedObj = (Manager::GetCurrentScene()) ? Manager::GetCurrentScene()->GetSelectedGameObject() : nullptr;
		Shader* selectedShader = selectedObj ? selectedObj->GetComponent<Shader>() : nullptr;
		if (!selectedShader) {
			ImGui::BeginDisabled(true);
		}
		if (ImGui::Button("Export & Apply to Selected Object", ImVec2(-1.0f, 28.0f))) {
			ExportProceduralTextures(true);
		}
		if (!selectedShader) {
			ImGui::EndDisabled();
			ImGui::TextDisabled("(Select a GameObject with Shader to apply)");
		} else {
			ImGui::TextColored(ImVec4(0.55f, 0.80f, 1.0f, 1.0f), "Target: %s", selectedObj->_name.c_str());
		}
	}
	ImGui::EndChild();

	ImGui::End();
}

