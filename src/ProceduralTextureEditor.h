//=====================================================================================
// ProceduralTextureEditor.h
// Author:Kaito Aoki
// Date:2026/10/26
//=====================================================================================

#ifndef _PROCEDURAL_TEXTURE_EDITOR_H_
#define _PROCEDURAL_TEXTURE_EDITOR_H_

#include <string>
#include <vector>
#include "imgui.h"
#include "Renderer.h"

enum class ProcNodeType : int {
	// Generators
	PerlinNoise = 0,
	Voronoi,
	Checker,
	Brick,
	Gradient,
	ConstantColor,

	// Filters / Operators
	Blend,
	ColorRamp,
	UVWarp,
	Levels,
	NormalMap,

	// Output
	Output
};

struct ProcNodeLink {
	int id = 0;
	int fromNodeId = -1;
	int fromPinIdx = 0;
	int toNodeId = -1;
	int toPinIdx = 0;

	template <class Archive>
	void serialize(Archive& ar) {
		ar(id, fromNodeId, fromPinIdx, toNodeId, toPinIdx);
	}
};

struct ProcNode {
	int id = 0;
	ProcNodeType type = ProcNodeType::PerlinNoise;
	std::string title;
	ImVec2 pos = ImVec2(100.0f, 100.0f);

	// Node parameters
	float scaleX = 4.0f;
	float scaleY = 4.0f;
	int octaves = 4;
	float persistence = 0.5f;
	float lacunarity = 2.0f;
	int seed = 1337;
	int mode = 0;           // Sub-mode (e.g., Voronoi mode, Blend mode, Gradient mode)
	bool flagA = false;     // Invert / Ridged / GridMode / UseMidStop

	float paramA = 0.5f;    // Jitter / MortarSize / Opacity / Strength / Brightness / Angle
	float paramB = 0.5f;    // RowOffset / Contrast / Stop1Pos
	float paramC = 1.0f;    // BrickVariation / Gamma / Stop2Pos
	float paramD = 0.0f;    // Stop0Pos / Metallic fallback

	float colorA[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	float colorB[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	float colorC[4] = { 0.5f, 0.5f, 0.5f, 1.0f };

	// Runtime evaluation cache (not serialized)
	std::vector<float> bufferRGBA; // size = evalWidth * evalHeight * 4
	int evalWidth = 0;
	int evalHeight = 0;
	const TEXTURE* previewTexture = nullptr;

	template <class Archive>
	void serialize(Archive& ar) {
		int typeInt = static_cast<int>(type);
		ar(id, typeInt, title, pos.x, pos.y,
		   scaleX, scaleY, octaves, persistence, lacunarity, seed, mode, flagA,
		   paramA, paramB, paramC, paramD,
		   colorA[0], colorA[1], colorA[2], colorA[3],
		   colorB[0], colorB[1], colorB[2], colorB[3],
		   colorC[0], colorC[1], colorC[2], colorC[3]);
		type = static_cast<ProcNodeType>(typeInt);
	}
};

class ProceduralTextureEditor {
public:
	static void Initialize();
	static void RenderUI(bool* p_open = nullptr);
};

#endif // !_PROCEDURAL_TEXTURE_EDITOR_H_

