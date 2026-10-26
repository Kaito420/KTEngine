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
	s_selectedNodeId = outId;
	s_graphDirty = true;
}

// =====================================================================================
// Node Evaluation Logic
// =====================================================================================
static void EvaluateSingleNode(ProcNode& node, int width, int height) {
	node.evalWidth = width;
	node.evalHeight = height;
	node.bufferRGBA.resize(static_cast<size_t>(width) * height * 4);

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
				float val = EvaluateFBM(u, v, node.scaleX, node.scaleY, node.octaves, node.persistence, node.lacunarity, node.seed, node.flagA);
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
				float val = EvaluateVoronoi(u, v, node.scaleX, node.paramA, node.mode, node.seed);
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
				float sx = u * std::max(1.0f, node.scaleX);
				float sy = v * std::max(1.0f, node.scaleY);
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
				float rx = u * cols + ((rowIdx & 1) ? node.paramB : 0.0f);
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
				if (node.mode == 0) {
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
				if (srcNode && static_cast<int>(srcNode->bufferRGBA.size()) == width * height * 4) {
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
	}
}

// =====================================================================================
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
	EvaluateGraph(exportRes, exportRes, false);

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
	EvaluateGraph(s_previewResolution, s_previewResolution, true);

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
	LoadPresetOutputOnly();
}

void ProceduralTextureEditor::RenderUI(bool* p_open) {
	if (!s_initialized) {
		Initialize();
	}

	if (s_graphDirty) {
		EvaluateGraph(s_previewResolution, s_previewResolution, true);
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
				s_selectedNodeId = node.id;
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
				s_selectedNodeId = node.id;
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
			ImGui::EndPopup();
		}

		// Overlay hint at bottom-left of canvas
		drawList->AddText(
			ImVec2(canvasPos.x + 10.0f, canvasPos.y + canvasSize.y - 22.0f),
			IM_COL32(160, 165, 175, 180),
			"Middle-Drag: Pan | Right-Click Canvas: Add Node | Right-Click Input Pin: Disconnect"
		);
	}
	ImGui::EndChild();

	ImGui::SameLine();

	// ---------------------------------------------------------------------------------
	// RIGHT PANE: Live Preview & PNG Export Panel
	// ---------------------------------------------------------------------------------
	ImGui::BeginChild("PreviewAndExportPane", ImVec2(rightPaneWidth, avail.y), true);
	{
		ImGui::TextUnformatted("2D Texture Preview");
		const char* channelNames[] = { "Output Albedo", "Output Normal", "Output ARM", "Selected Node" };
		ImGui::SetNextItemWidth(-1.0f);
		ImGui::Combo("##PreviewChannel", &s_previewChannel, channelNames, IM_ARRAYSIZE(channelNames));

		ImGui::Checkbox("2x2 Tiling Preview", &s_tilePreview2x2);

		const TEXTURE* previewTex = s_outAlbedoTex;
		if (s_previewChannel == 1) previewTex = s_outNormalTex;
		else if (s_previewChannel == 2) previewTex = s_outArmTex;
		else if (s_previewChannel == 3) {
			ProcNode* sel = FindNode(s_selectedNodeId);
			if (sel && sel->previewTexture) previewTex = sel->previewTexture;
		}

		float previewSize = rightPaneWidth - 20.0f;
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

