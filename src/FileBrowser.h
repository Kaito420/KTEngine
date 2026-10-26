//=====================================================================================
// FileBrowser.h
// Author:Kaito Aoki
// Date:2025/06/23
//=====================================================================================

#ifndef _FILEBROWSER_H
#define _FILEBROWSER_H

#include <filesystem>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <cereal/archives/json.hpp>
#include <cereal/types/string.hpp>
#include "imgui.h"
#include "Renderer.h"
#include "Texture.h"
#include "ModelRenderer.h"
#include "Manager.h"

namespace fs = std::filesystem;

enum class BrowserIconCategory : int {
	Folder = 0,
	Json   = 1,
	Ini    = 2,
	Cpp    = 3,
	Header = 4,
	Other  = 5,
	Count  = 6
};

class FileBrowser {
private:
	fs::path _currentPath;
	float _thumbnailSize = 64.0f;

	// Custom icon paths assigned by user per category
	std::string _customIconPaths[static_cast<int>(BrowserIconCategory::Count)];
	bool _configLoaded = false;
	bool _defaultIconsInitialized = false;

	// Default procedural icon textures
	const TEXTURE* _defaultIcons[static_cast<int>(BrowserIconCategory::Count)] = { nullptr };
	const TEXTURE* _defaultModelFallbackIcon = nullptr;

	// Cache for generated 3D model thumbnails and attempted loads
	std::unordered_map<std::string, const TEXTURE*> _thumbnailCache;
	std::unordered_set<std::string> _attemptedPaths;

	static constexpr const char* ICON_CONFIG_FILE = "content_browser_icons.json";

	static bool IsImageExtension(const std::string& ext) {
		return ext == ".png" || ext == ".jpg" || ext == ".jpeg" ||
		       ext == ".tga" || ext == ".dds" || ext == ".bmp";
	}

	static bool IsModelExtension(const std::string& ext) {
		return ext == ".obj" || ext == ".fbx";
	}

	static const char* GetCategoryLabel(BrowserIconCategory cat) {
		switch (cat) {
		case BrowserIconCategory::Folder: return "Folder (Directory)";
		case BrowserIconCategory::Json:   return "JSON / Scene (.json)";
		case BrowserIconCategory::Ini:    return "Config (.ini)";
		case BrowserIconCategory::Cpp:    return "C++ Source (.cpp)";
		case BrowserIconCategory::Header: return "Header (.h / .hpp)";
		case BrowserIconCategory::Other:  return "Other Files";
		default:                          return "Unknown";
		}
	}

	void LoadIconConfig() {
		if (_configLoaded) return;
		_configLoaded = true;

		std::ifstream ifs(ICON_CONFIG_FILE);
		if (!ifs.is_open()) return;

		try {
			cereal::JSONInputArchive ar(ifs);
			try { ar(cereal::make_nvp("FolderIcon", _customIconPaths[(int)BrowserIconCategory::Folder])); } catch (...) {}
			try { ar(cereal::make_nvp("JsonIcon",   _customIconPaths[(int)BrowserIconCategory::Json])); }   catch (...) {}
			try { ar(cereal::make_nvp("IniIcon",    _customIconPaths[(int)BrowserIconCategory::Ini])); }    catch (...) {}
			try { ar(cereal::make_nvp("CppIcon",    _customIconPaths[(int)BrowserIconCategory::Cpp])); }    catch (...) {}
			try { ar(cereal::make_nvp("HeaderIcon", _customIconPaths[(int)BrowserIconCategory::Header])); } catch (...) {}
			try { ar(cereal::make_nvp("OtherIcon",  _customIconPaths[(int)BrowserIconCategory::Other])); }  catch (...) {}
		} catch (...) {}
	}

	void SaveIconConfig() const {
		std::ofstream ofs(ICON_CONFIG_FILE);
		if (!ofs.is_open()) return;

		try {
			cereal::JSONOutputArchive ar(ofs);
			ar(cereal::make_nvp("FolderIcon", _customIconPaths[(int)BrowserIconCategory::Folder]));
			ar(cereal::make_nvp("JsonIcon",   _customIconPaths[(int)BrowserIconCategory::Json]));
			ar(cereal::make_nvp("IniIcon",    _customIconPaths[(int)BrowserIconCategory::Ini]));
			ar(cereal::make_nvp("CppIcon",    _customIconPaths[(int)BrowserIconCategory::Cpp]));
			ar(cereal::make_nvp("HeaderIcon", _customIconPaths[(int)BrowserIconCategory::Header]));
			ar(cereal::make_nvp("OtherIcon",  _customIconPaths[(int)BrowserIconCategory::Other]));
		} catch (...) {}
	}

	// Helper to set pixel in 64x64 RGBA buffer with alpha blending
	static void SetPixelRGBA(std::vector<unsigned char>& buf, int w, int h, int x, int y,
	                         unsigned char r, unsigned char g, unsigned char b, unsigned char a = 255) {
		if (x < 0 || x >= w || y < 0 || y >= h) return;
		int idx = (y * w + x) * 4;
		if (a == 255) {
			buf[idx + 0] = r;
			buf[idx + 1] = g;
			buf[idx + 2] = b;
			buf[idx + 3] = 255;
		} else if (a > 0) {
			float alpha = a / 255.0f;
			float inv = 1.0f - alpha;
			buf[idx + 0] = (unsigned char)(r * alpha + buf[idx + 0] * inv);
			buf[idx + 1] = (unsigned char)(g * alpha + buf[idx + 1] * inv);
			buf[idx + 2] = (unsigned char)(b * alpha + buf[idx + 2] * inv);
			buf[idx + 3] = (unsigned char)(std::min)(255.0f, buf[idx + 3] + a * inv);
		}
	}

	static void FillRectRGBA(std::vector<unsigned char>& buf, int w, int h,
	                         int x0, int y0, int x1, int y1,
	                         unsigned char r, unsigned char g, unsigned char b, unsigned char a = 255) {
		for (int y = y0; y <= y1; y++) {
			for (int x = x0; x <= x1; x++) {
				SetPixelRGBA(buf, w, h, x, y, r, g, b, a);
			}
		}
	}

	// Simple 5x7 bitmap font renderer for icon badges ("JSON", "INI", "C++", "H", "3D")
	static void DrawGlyph5x7(std::vector<unsigned char>& buf, int w, int h, int ox, int oy, int scale,
	                         char ch, unsigned char r, unsigned char g, unsigned char b) {
		static const unsigned char fontC[7] = { 0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E };
		static const unsigned char fontP[7] = { 0x04, 0x04, 0x1F, 0x04, 0x04, 0x00, 0x00 }; // '+'
		static const unsigned char fontH[7] = { 0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11 };
		static const unsigned char fontJ[7] = { 0x07, 0x02, 0x02, 0x02, 0x12, 0x12, 0x0C };
		static const unsigned char fontS[7] = { 0x0E, 0x11, 0x10, 0x0E, 0x01, 0x11, 0x0E };
		static const unsigned char fontO[7] = { 0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E };
		static const unsigned char fontN[7] = { 0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11 };
		static const unsigned char fontI[7] = { 0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E };
		static const unsigned char font3[7] = { 0x0E, 0x11, 0x01, 0x06, 0x01, 0x11, 0x0E };
		static const unsigned char fontD[7] = { 0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E };

		const unsigned char* glyph = nullptr;
		switch (ch) {
		case 'C': glyph = fontC; break;
		case '+': glyph = fontP; break;
		case 'H': glyph = fontH; break;
		case 'J': glyph = fontJ; break;
		case 'S': glyph = fontS; break;
		case 'O': glyph = fontO; break;
		case 'N': glyph = fontN; break;
		case 'I': glyph = fontI; break;
		case '3': glyph = font3; break;
		case 'D': glyph = fontD; break;
		default: return;
		}

		for (int row = 0; row < 7; row++) {
			for (int col = 0; col < 5; col++) {
				if (glyph[row] & (1 << (4 - col))) {
					FillRectRGBA(buf, w, h,
						ox + col * scale, oy + row * scale,
						ox + (col + 1) * scale - 1, oy + (row + 1) * scale - 1,
						r, g, b, 255);
				}
			}
		}
	}

	static void DrawText5x7(std::vector<unsigned char>& buf, int w, int h, int ox, int oy, int scale,
	                        const char* text, unsigned char r, unsigned char g, unsigned char b) {
		int curX = ox;
		for (int i = 0; text[i] != '\0'; i++) {
			DrawGlyph5x7(buf, w, h, curX, oy, scale, text[i], r, g, b);
			curX += 6 * scale;
		}
	}

	static const TEXTURE* CreateDocumentIconTexture(
		const std::string& key,
		unsigned char bodyR, unsigned char bodyG, unsigned char bodyB,
		unsigned char badgeR, unsigned char badgeG, unsigned char badgeB,
		const char* badgeText)
	{
		const int W = 64;
		const int H = 64;
		std::vector<unsigned char> buf(W * H * 4, 0);

		// Document body with top-right folded corner
		const int left = 10, right = 53, top = 6, bottom = 58, fold = 12;
		for (int y = top; y <= bottom; y++) {
			for (int x = left; x <= right; x++) {
				if (x > right - fold && y < top + fold) {
					if ((x - (right - fold)) > (y - top)) continue;
				}
				// Subtle vertical shading
				float shade = 1.0f - 0.12f * ((float)(y - top) / (float)(bottom - top));
				SetPixelRGBA(buf, W, H, x, y,
					(unsigned char)(bodyR * shade),
					(unsigned char)(bodyG * shade),
					(unsigned char)(bodyB * shade), 255);
			}
		}

		// Folded corner triangle
		for (int y = top; y < top + fold; y++) {
			for (int x = right - fold; x <= right; x++) {
				if ((x - (right - fold)) <= (y - top)) {
					SetPixelRGBA(buf, W, H, x, y,
						(unsigned char)(std::min)(255, bodyR + 35),
						(unsigned char)(std::min)(255, bodyG + 35),
						(unsigned char)(std::min)(255, bodyB + 35), 255);
				}
			}
		}

		// Subtle content lines on upper half
		FillRectRGBA(buf, W, H, 16, 18, 36, 20, 255, 255, 255, 90);
		FillRectRGBA(buf, W, H, 16, 24, 44, 26, 255, 255, 255, 70);
		FillRectRGBA(buf, W, H, 16, 30, 40, 32, 255, 255, 255, 70);

		// Color badge banner on lower half
		FillRectRGBA(buf, W, H, 6, 36, 57, 52, badgeR, badgeG, badgeB, 255);

		if (badgeText && badgeText[0] != '\0') {
			int len = (int)strlen(badgeText);
			int scale = (len <= 3) ? 2 : 1;
			int textW = len * 6 * scale - scale;
			int startX = (W - textW) / 2;
			int startY = 36 + (17 - 7 * scale) / 2;
			DrawText5x7(buf, W, H, startX, startY, scale, badgeText, 255, 255, 255);
		}

		return Texture::CreateFromRGBA(key, buf.data(), W, H);
	}

	static const TEXTURE* CreateFolderIconTexture(const std::string& key) {
		const int W = 64;
		const int H = 64;
		std::vector<unsigned char> buf(W * H * 4, 0);

		// Back tab of folder
		FillRectRGBA(buf, W, H, 6, 12, 28, 20, 215, 155, 40, 255);
		FillRectRGBA(buf, W, H, 6, 16, 57, 52, 200, 140, 35, 255);

		// Inner paper sheet peek
		FillRectRGBA(buf, W, H, 10, 19, 53, 25, 245, 245, 250, 255);

		// Front flap of folder with gradient
		for (int y = 23; y <= 54; y++) {
			float t = (float)(y - 23) / 31.0f;
			unsigned char r = (unsigned char)(250 - t * 35);
			unsigned char g = (unsigned char)(195 - t * 40);
			unsigned char b = (unsigned char)(65 - t * 20);
			FillRectRGBA(buf, W, H, 5, y, 58, y, r, g, b, 255);
		}

		// Top highlight line on front flap
		FillRectRGBA(buf, W, H, 5, 23, 58, 24, 255, 225, 120, 255);

		return Texture::CreateFromRGBA(key, buf.data(), W, H);
	}

	void EnsureDefaultIcons() {
		if (_defaultIconsInitialized) return;
		if (!Renderer::GetDeviceDX12()) return;
		_defaultIconsInitialized = true;

		_defaultIcons[(int)BrowserIconCategory::Folder] = CreateFolderIconTexture("__cb_default_folder__");
		_defaultIcons[(int)BrowserIconCategory::Json]   = CreateDocumentIconTexture("__cb_default_json__",   45, 65, 55,  40, 165, 85,  "JSON");
		_defaultIcons[(int)BrowserIconCategory::Ini]    = CreateDocumentIconTexture("__cb_default_ini__",    65, 55, 45,  210, 125, 35, "INI");
		_defaultIcons[(int)BrowserIconCategory::Cpp]    = CreateDocumentIconTexture("__cb_default_cpp__",    45, 55, 75,  45, 115, 215, "C++");
		_defaultIcons[(int)BrowserIconCategory::Header] = CreateDocumentIconTexture("__cb_default_header__", 60, 45, 75,  145, 60, 190, "H");
		_defaultIcons[(int)BrowserIconCategory::Other]  = CreateDocumentIconTexture("__cb_default_other__",  65, 68, 75,  110, 115, 125, "");
		_defaultModelFallbackIcon                       = CreateDocumentIconTexture("__cb_default_3d__",     40, 65, 75,  30, 160, 185, "3D");
	}

	// Generate 3D Model Thumbnail using CPU Software Rasterizer with Z-buffer & 3-point studio lighting
	const TEXTURE* GenerateModelThumbnail(const std::string& pathStr) {
		const MODEL* model = ModelRenderer::GetOrLoadModel(pathStr.c_str());
		if (!model || model->BaseVertices.empty() || model->BaseIndices.empty()) {
			return _defaultModelFallbackIcon;
		}

		const int W = 128;
		const int H = 128;
		std::vector<unsigned char> pixels(W * H * 4, 0);
		std::vector<float> zbuf(W * H, 1.0e30f);

		// Studio dark gradient background
		for (int y = 0; y < H; y++) {
			float t = (float)y / (float)(H - 1);
			unsigned char bgR = (unsigned char)(46 - t * 18);
			unsigned char bgG = (unsigned char)(50 - t * 18);
			unsigned char bgB = (unsigned char)(62 - t * 20);
			for (int x = 0; x < W; x++) {
				int idx = (y * W + x) * 4;
				pixels[idx + 0] = bgR;
				pixels[idx + 1] = bgG;
				pixels[idx + 2] = bgB;
				pixels[idx + 3] = 255;
			}
		}

		// Compute AABB of the mesh
		const auto& verts = model->BaseVertices;
		const auto& indices = model->BaseIndices;

		XMFLOAT3 minP = verts[0].position;
		XMFLOAT3 maxP = verts[0].position;
		for (const auto& v : verts) {
			if (v.position.x < minP.x) minP.x = v.position.x;
			if (v.position.y < minP.y) minP.y = v.position.y;
			if (v.position.z < minP.z) minP.z = v.position.z;
			if (v.position.x > maxP.x) maxP.x = v.position.x;
			if (v.position.y > maxP.y) maxP.y = v.position.y;
			if (v.position.z > maxP.z) maxP.z = v.position.z;
		}

		XMFLOAT3 center = {
			(minP.x + maxP.x) * 0.5f,
			(minP.y + maxP.y) * 0.5f,
			(minP.z + maxP.z) * 0.5f
		};
		float dx = maxP.x - minP.x;
		float dy = maxP.y - minP.y;
		float dz = maxP.z - minP.z;
		float radius = std::sqrt(dx * dx + dy * dy + dz * dz) * 0.5f;
		if (radius < 0.0001f) radius = 1.0f;

		// Isometric-like perspective camera framing the bounding sphere
		float dist = radius * 2.15f;
		XMVECTOR targetVec = XMLoadFloat3(&center);
		XMVECTOR offsetVec = XMVector3Normalize(XMVectorSet(-0.65f, 0.50f, -0.75f, 0.0f)) * dist;
		XMVECTOR eyeVec = XMVectorAdd(targetVec, offsetVec);
		XMVECTOR upVec = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

		XMMATRIX view = XMMatrixLookAtLH(eyeVec, targetVec, upVec);
		XMMATRIX proj = XMMatrixPerspectiveFovLH(XMConvertToRadians(38.0f), 1.0f, radius * 0.05f, radius * 10.0f);
		XMMATRIX viewProj = view * proj;

		// Pre-transform all vertices to screen space
		struct ProjVertex {
			float sx, sy, sz, w;
			XMFLOAT3 normal;
		};
		std::vector<ProjVertex> pverts(verts.size());
		for (size_t i = 0; i < verts.size(); i++) {
			XMVECTOR p = XMVectorSet(verts[i].position.x, verts[i].position.y, verts[i].position.z, 1.0f);
			XMVECTOR clip = XMVector4Transform(p, viewProj);
			float cw = XMVectorGetW(clip);
			pverts[i].w = cw;
			if (cw > 0.0001f) {
				float invW = 1.0f / cw;
				float ndcX = XMVectorGetX(clip) * invW;
				float ndcY = XMVectorGetY(clip) * invW;
				float ndcZ = XMVectorGetZ(clip) * invW;
				pverts[i].sx = (ndcX * 0.5f + 0.5f) * (float)(W - 1);
				pverts[i].sy = (1.0f - (ndcY * 0.5f + 0.5f)) * (float)(H - 1);
				pverts[i].sz = ndcZ;
			}
			pverts[i].normal = verts[i].normal;
		}

		// Studio lights
		XMVECTOR keyLight = XMVector3Normalize(XMVectorSet(-0.55f, 0.70f, -0.60f, 0.0f));
		XMVECTOR fillLight = XMVector3Normalize(XMVectorSet(0.65f, -0.25f, -0.50f, 0.0f));

		auto rasterizeRange = [&](unsigned int startIdx, unsigned int indexCount, XMFLOAT3 baseCol) {
			unsigned int endIdx = startIdx + indexCount;
			if (endIdx > indices.size()) endIdx = (unsigned int)indices.size();

			for (unsigned int i = startIdx; i + 2 < endIdx; i += 3) {
				unsigned int i0 = indices[i];
				unsigned int i1 = indices[i + 1];
				unsigned int i2 = indices[i + 2];
				if (i0 >= pverts.size() || i1 >= pverts.size() || i2 >= pverts.size()) continue;

				const ProjVertex& v0 = pverts[i0];
				const ProjVertex& v1 = pverts[i1];
				const ProjVertex& v2 = pverts[i2];
				if (v0.w <= 0.0001f || v1.w <= 0.0001f || v2.w <= 0.0001f) continue;

				// 2D signed area
				float area = (v1.sx - v0.sx) * (v2.sy - v0.sy) - (v1.sy - v0.sy) * (v2.sx - v0.sx);
				if (std::fabs(area) < 1e-4f) continue;

				// Compute triangle normal
				XMVECTOR n0 = XMLoadFloat3(&v0.normal);
				XMVECTOR n1 = XMLoadFloat3(&v1.normal);
				XMVECTOR n2 = XMLoadFloat3(&v2.normal);
				XMVECTOR avgN = n0 + n1 + n2;
				if (XMVectorGetX(XMVector3LengthSq(avgN)) < 1e-4f) {
					XMVECTOR p0 = XMLoadFloat3(&verts[i0].position);
					XMVECTOR p1 = XMLoadFloat3(&verts[i1].position);
					XMVECTOR p2 = XMLoadFloat3(&verts[i2].position);
					avgN = XMVector3Cross(p1 - p0, p2 - p0);
				}
				avgN = XMVector3Normalize(avgN);

				// Two-sided lighting so flipped normals still look good
				float ndotlKey = std::fabs(XMVectorGetX(XMVector3Dot(avgN, keyLight)));
				float ndotlFill = (std::max)(0.0f, XMVectorGetX(XMVector3Dot(avgN, fillLight)));
				float lighting = 0.28f + 0.65f * ndotlKey + 0.22f * ndotlFill;
				float spec = std::pow(ndotlKey, 16.0f) * 0.25f;

				unsigned char r = (unsigned char)(std::min)(255.0f, (baseCol.x * lighting + spec) * 255.0f);
				unsigned char g = (unsigned char)(std::min)(255.0f, (baseCol.y * lighting + spec) * 255.0f);
				unsigned char b = (unsigned char)(std::min)(255.0f, (baseCol.z * lighting + spec) * 255.0f);

				int minX = (std::max)(0, (int)std::floor((std::min)({ v0.sx, v1.sx, v2.sx })));
				int maxX = (std::min)(W - 1, (int)std::ceil((std::max)({ v0.sx, v1.sx, v2.sx })));
				int minY = (std::max)(0, (int)std::floor((std::min)({ v0.sy, v1.sy, v2.sy })));
				int maxY = (std::min)(H - 1, (int)std::ceil((std::max)({ v0.sy, v1.sy, v2.sy })));

				float invArea = 1.0f / area;
				for (int py = minY; py <= maxY; py++) {
					float fy = (float)py + 0.5f;
					for (int px = minX; px <= maxX; px++) {
						float fx = (float)px + 0.5f;

						float w0 = ((v1.sx - fx) * (v2.sy - fy) - (v1.sy - fy) * (v2.sx - fx)) * invArea;
						float w1 = ((v2.sx - fx) * (v0.sy - fy) - (v2.sy - fy) * (v0.sx - fx)) * invArea;
						float w2 = 1.0f - w0 - w1;

						if (w0 >= 0.0f && w1 >= 0.0f && w2 >= 0.0f) {
							float z = w0 * v0.sz + w1 * v1.sz + w2 * v2.sz;
							int pIdx = py * W + px;
							if (z < zbuf[pIdx]) {
								zbuf[pIdx] = z;
								int cIdx = pIdx * 4;
								pixels[cIdx + 0] = r;
								pixels[cIdx + 1] = g;
								pixels[cIdx + 2] = b;
								pixels[cIdx + 3] = 255;
							}
						}
					}
				}
			}
		};

		if (model->SubsetNum > 0 && model->SubsetArray != nullptr) {
			for (unsigned int s = 0; s < model->SubsetNum; s++) {
				XMFLOAT4 diff = model->SubsetArray[s].Material.Material.Diffuse;
				XMFLOAT3 baseCol = { diff.x, diff.y, diff.z };
				if (baseCol.x + baseCol.y + baseCol.z < 0.08f) {
					baseCol = { 0.72f, 0.78f, 0.86f };
				}
				rasterizeRange(model->SubsetArray[s].StartIndex, model->SubsetArray[s].IndexNum, baseCol);
			}
		} else {
			rasterizeRange(0, (unsigned int)indices.size(), { 0.72f, 0.78f, 0.86f });
		}

		// Small "3D" badge in bottom-right corner
		FillRectRGBA(pixels, W, H, W - 32, H - 18, W - 4, H - 4, 25, 135, 165, 220);
		DrawText5x7(pixels, W, H, W - 28, H - 15, 1, "3D", 255, 255, 255);

		std::string texKey = "__cb_model_thumb__:" + pathStr;
		return Texture::CreateFromRGBA(texKey, pixels.data(), W, H);
	}

	const TEXTURE* GetIconForCategory(BrowserIconCategory cat) {
		EnsureDefaultIcons();
		int idx = static_cast<int>(cat);
		if (!(_customIconPaths[idx].empty())) {
			const TEXTURE* customTex = Texture::Load(_customIconPaths[idx].c_str());
			if (customTex) return customTex;
		}
		return _defaultIcons[idx];
	}

	const TEXTURE* GetEntryThumbnail(const fs::path& path, bool isDir, const std::string& ext, int& loadsThisFrame) {
		EnsureDefaultIcons();

		if (isDir) {
			return GetIconForCategory(BrowserIconCategory::Folder);
		}

		std::string genericPath = path.generic_string();

		// 1. Image files -> load their own image
		if (IsImageExtension(ext)) {
			auto it = _thumbnailCache.find(genericPath);
			if (it != _thumbnailCache.end()) {
				return it->second ? it->second : GetIconForCategory(BrowserIconCategory::Other);
			}
			if (loadsThisFrame < 2) {
				loadsThisFrame++;
				const TEXTURE* tex = Texture::Load(genericPath.c_str());
				_thumbnailCache[genericPath] = tex;
				if (tex) return tex;
			}
			return GetIconForCategory(BrowserIconCategory::Other);
		}

		// 2. 3D Model files -> render 3D mesh preview
		if (IsModelExtension(ext)) {
			auto it = _thumbnailCache.find(genericPath);
			if (it != _thumbnailCache.end()) {
				return it->second ? it->second : _defaultModelFallbackIcon;
			}
			if (loadsThisFrame < 1) {
				loadsThisFrame++;
				const TEXTURE* tex = GenerateModelThumbnail(genericPath);
				_thumbnailCache[genericPath] = tex;
				if (tex) return tex;
			}
			return _defaultModelFallbackIcon;
		}

		// 3. Other known file extensions -> Custom icon or default icon
		if (ext == ".json") {
			return GetIconForCategory(BrowserIconCategory::Json);
		}
		if (ext == ".ini") {
			return GetIconForCategory(BrowserIconCategory::Ini);
		}
		if (ext == ".cpp" || ext == ".c" || ext == ".cc" || ext == ".cxx") {
			return GetIconForCategory(BrowserIconCategory::Cpp);
		}
		if (ext == ".h" || ext == ".hpp" || ext == ".inl") {
			return GetIconForCategory(BrowserIconCategory::Header);
		}

		return GetIconForCategory(BrowserIconCategory::Other);
	}

	std::vector<std::string> CollectAvailableImages() const {
		std::vector<std::string> images;
		std::vector<std::string> dirs = { "asset/texture", "asset/model", "asset" };
		for (const auto& d : dirs) {
			if (!fs::exists(d) || !fs::is_directory(d)) continue;
			try {
				for (const auto& entry : fs::directory_iterator(d)) {
					if (!entry.is_regular_file()) continue;
					std::string ext = entry.path().extension().string();
					std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
					if (IsImageExtension(ext)) {
						std::string rel = entry.path().generic_string();
						if (std::find(images.begin(), images.end(), rel) == images.end()) {
							images.push_back(rel);
						}
					}
				}
			} catch (...) {}
		}
		return images;
	}

	void RenderIconSettingsPopup() {
		if (ImGui::BeginPopup("ContentBrowserIconSettingsPopup")) {
			ImGui::TextUnformatted("Custom Icon Settings");
			ImGui::TextDisabled("Select custom images for folders and file types:");
			ImGui::Separator();

			std::vector<std::string> availableImages = CollectAvailableImages();

			for (int i = 0; i < (int)BrowserIconCategory::Count; i++) {
				BrowserIconCategory cat = static_cast<BrowserIconCategory>(i);
				ImGui::PushID(i);

				const TEXTURE* previewTex = GetIconForCategory(cat);
				void* srv = Renderer::GetTextureSRV(previewTex);
				if (srv) {
					ImGui::Image(srv, ImVec2(22, 22));
					ImGui::SameLine();
				}

				std::string& currentPath = _customIconPaths[i];
				std::string displayLabel = currentPath.empty()
					? "Default (Built-in)"
					: fs::path(currentPath).filename().string();

				ImGui::SetNextItemWidth(220.0f);
				if (ImGui::BeginCombo(GetCategoryLabel(cat), displayLabel.c_str())) {
					if (ImGui::Selectable("Default (Built-in)", currentPath.empty())) {
						currentPath = "";
						SaveIconConfig();
					}
					for (const auto& imgPath : availableImages) {
						bool selected = (currentPath == imgPath);
						if (ImGui::Selectable(imgPath.c_str(), selected)) {
							currentPath = imgPath;
							SaveIconConfig();
						}
						if (selected) {
							ImGui::SetItemDefaultFocus();
						}
					}
					ImGui::EndCombo();
				}

				ImGui::PopID();
			}

			ImGui::Separator();
			if (ImGui::Button("Reset All to Default")) {
				for (int i = 0; i < (int)BrowserIconCategory::Count; i++) {
					_customIconPaths[i].clear();
				}
				SaveIconConfig();
			}
			ImGui::SameLine();
			if (ImGui::Button("Refresh 3D Thumbnails")) {
				_thumbnailCache.clear();
			}

			ImGui::EndPopup();
		}
	}

public:
	FileBrowser() {
		_currentPath = fs::current_path();
	}

	void Render() {
		LoadIconConfig();
		EnsureDefaultIcons();

		ImGui::Begin("Content Browser");
		{
			if (ImGui::Button("<-")) {
				if (_currentPath.has_parent_path()) {
					_currentPath = _currentPath.parent_path();
				}
			}
			ImGui::SameLine();
			ImGui::Text("%s", _currentPath.string().c_str());

			// Right-aligned Icon Settings button & Thumbnail size slider
			float rightControlsWidth = 230.0f;
			if (ImGui::GetContentRegionAvail().x > rightControlsWidth) {
				ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - rightControlsWidth);
			} else {
				ImGui::SameLine();
			}
			ImGui::SetNextItemWidth(100.0f);
			ImGui::SliderFloat("##ThumbSize", &_thumbnailSize, 48.0f, 128.0f, "%.0fpx");
			ImGui::SameLine();
			if (ImGui::Button("Icon Settings")) {
				ImGui::OpenPopup("ContentBrowserIconSettingsPopup");
			}
			RenderIconSettingsPopup();

			ImGui::Separator();

			float padding = 16.0f;
			float thumbnailSize = _thumbnailSize;
			float cellSize = thumbnailSize + padding;

			float panelWidth = ImGui::GetContentRegionAvail().x;
			int columnCount = (int)(panelWidth / cellSize);
			if (columnCount < 1) columnCount = 1;

			ImGui::Columns(columnCount, 0, false);

			int loadsThisFrame = 0;

			try {
				for (const auto& entry : fs::directory_iterator(_currentPath)) {
					const auto& path = entry.path();
					std::string filename = path.filename().string();
					std::string extension = path.extension().string();
					std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

					bool isDir = entry.is_directory();

					const TEXTURE* thumbTex = GetEntryThumbnail(path, isDir, extension, loadsThisFrame);
					void* texSrv = Renderer::GetTextureSRV(thumbTex);

					ImGui::PushID(filename.c_str());

					bool clicked = false;
					if (texSrv != nullptr) {
						ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.16f, 0.18f, 0.22f, 0.65f));
						ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.28f, 0.34f, 0.44f, 0.90f));
						ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.22f, 0.28f, 0.38f, 1.00f));
						clicked = ImGui::ImageButton("##thumb", texSrv, ImVec2(thumbnailSize, thumbnailSize));
						ImGui::PopStyleColor(3);
					} else {
						clicked = ImGui::Button(isDir ? "DIR" : "FILE", ImVec2(thumbnailSize, thumbnailSize));
					}

					// Right-click on any image file to quickly assign it as a custom icon
					if (!isDir && IsImageExtension(extension)) {
						if (ImGui::BeginPopupContextItem("ImageContextPopup")) {
							if (ImGui::BeginMenu("Set as Custom Icon for...")) {
								for (int c = 0; c < (int)BrowserIconCategory::Count; c++) {
									BrowserIconCategory cat = static_cast<BrowserIconCategory>(c);
									if (ImGui::MenuItem(GetCategoryLabel(cat))) {
										_customIconPaths[c] = path.generic_string();
										SaveIconConfig();
									}
								}
								ImGui::EndMenu();
							}
							ImGui::EndPopup();
						}
					}

					if (isDir) {
						if (clicked) {
							_currentPath /= filename;
							ImGui::PopID();
							ImGui::Columns(1);
							ImGui::End();
							return;
						}
					} else {
						if (ImGui::IsItemHovered()) {
							ImGui::SetTooltip("%s", filename.c_str());
							if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
								if (extension == ".json") {
									Manager::OpenScene(path.string());
								}
							}
						}
					}

					ImGui::TextWrapped("%s", filename.c_str());

					ImGui::PopID();
					ImGui::NextColumn();
				}
			}
			catch (const std::exception& e) {
				ImGui::Columns(1);
				ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Error accessing directory: %s", e.what());
			}
			ImGui::Columns(1);

			ImGui::End();
		}
	}
};

#endif // !_FILEBROWSER_H
