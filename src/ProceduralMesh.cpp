//=====================================================================================
// ProceduralMesh.cpp
// Author: KTEngine
// Description: Implementation of ProceduralMesh (Subdivided Plane, PNG Displacement
//              Mapping, Procedural Noise Field, and ColliderMesh synchronization).
//=====================================================================================

#include "ProceduralMesh.h"
#include "GameObject.h"
#include "Shader.h"
#include "ShaderManager.h"
#include "Texture.h"
#include "Collider.h"
#include "Manager.h"
#include "Profiler.h"
#include "imgui.h"
#include <DirectXTex.h>
#include <filesystem>
#include <algorithm>
#include <cmath>
#include <deque>

namespace {
	// Keep retired GPU buffers alive across frames when sliders are dragged in ImGui
	static std::deque<std::unique_ptr<VERTEX_BUFFER>> s_retiredVBs;
	static std::deque<std::unique_ptr<INDEX_BUFFER>>  s_retiredIBs;

	void RetireBuffers(std::unique_ptr<VERTEX_BUFFER>& vb, std::unique_ptr<INDEX_BUFFER>& ib) {
		if (vb) {
			s_retiredVBs.push_back(std::move(vb));
			while (s_retiredVBs.size() > 24) {
				s_retiredVBs.pop_front();
			}
		}
		if (ib) {
			s_retiredIBs.push_back(std::move(ib));
			while (s_retiredIBs.size() > 24) {
				s_retiredIBs.pop_front();
			}
		}
	}

	float FadePerlin(float t) {
		return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
	}

	float Hash2D(int x, int y, int seed) {
		uint32_t h = static_cast<uint32_t>(x) * 374761393u
		           + static_cast<uint32_t>(y) * 668265263u
		           + static_cast<uint32_t>(seed) * 1442695041u;
		h = (h ^ (h >> 13u)) * 1274126177u;
		h = h ^ (h >> 16u);
		return static_cast<float>(h & 0x00FFFFFFu) / static_cast<float>(0x00FFFFFFu);
	}

	float GradientNoise2D(float x, float y, int seed) {
		int ix = static_cast<int>(std::floor(x));
		int iy = static_cast<int>(std::floor(y));
		float fx = x - static_cast<float>(ix);
		float fy = y - static_cast<float>(iy);

		float u = FadePerlin(fx);
		float v = FadePerlin(fy);

		auto gradDot = [&](int gx, int gy, float dx, float dy) -> float {
			float angle = Hash2D(gx, gy, seed) * 6.283185307f;
			return std::cos(angle) * dx + std::sin(angle) * dy;
		};

		float n00 = gradDot(ix,     iy,     fx,        fy);
		float n10 = gradDot(ix + 1, iy,     fx - 1.0f, fy);
		float n01 = gradDot(ix,     iy + 1, fx,        fy - 1.0f);
		float n11 = gradDot(ix + 1, iy + 1, fx - 1.0f, fy - 1.0f);

		float nx0 = n00 + (n10 - n00) * u;
		float nx1 = n01 + (n11 - n01) * u;
		return (nx0 + (nx1 - nx0) * v) * 0.7071f + 0.5f;
	}

	std::vector<std::string> GetProjectTextureFiles() {
		std::vector<std::string> files;
		namespace fs = std::filesystem;
		if (fs::exists("asset") && fs::is_directory("asset")) {
			std::error_code ec;
			for (const auto& entry : fs::recursive_directory_iterator("asset", fs::directory_options::skip_permission_denied, ec)) {
				if (entry.is_regular_file()) {
					std::string ext = entry.path().extension().string();
					std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
					if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga" || ext == ".dds" || ext == ".bmp") {
						files.push_back(entry.path().generic_string());
					}
				}
			}
		}
		std::sort(files.begin(), files.end());
		return files;
	}
}

void ProceduralMesh::Awake() {
	_executeInEditor = true;
	if (!_displacementMapPath.empty()) {
		LoadDisplacementMapCPU(_displacementMapPath);
	}
	RebuildMesh();
}

void ProceduralMesh::OnDestroy() {
	RetireBuffers(_vertexBuffer, _indexBuffer);
	_indexCount = 0;
}

void ProceduralMesh::Update() {
	if (_vertices.empty() || !_vertexBuffer || !_indexBuffer) {
		RebuildMesh();
	}
}

bool ProceduralMesh::SetDisplacementMapPath(const std::string& path) {
	_displacementMapPath = path;
	if (_displacementMapPath.empty()) {
		_dispPixels.clear();
		_dispWidth = 0;
		_dispHeight = 0;
		_displacementPreviewTex = nullptr;
		RebuildMesh();
		return true;
	}

	bool ok = LoadDisplacementMapCPU(_displacementMapPath);
	RebuildMesh();
	return ok;
}

bool ProceduralMesh::LoadDisplacementMapCPU(const std::string& path) {
	_dispPixels.clear();
	_dispWidth = 0;
	_dispHeight = 0;
	_displacementPreviewTex = nullptr;

	if (path.empty()) return false;

	namespace fs = std::filesystem;
	std::string resolvedPath = path;
	std::replace(resolvedPath.begin(), resolvedPath.end(), '\\', '/');

	if (!fs::is_regular_file(resolvedPath)) {
		std::string filename = fs::path(resolvedPath).filename().string();
		std::vector<std::string> searchDirs = { "asset/texture", "asset/model", "asset" };
		bool found = false;
		for (const auto& dir : searchDirs) {
			if (fs::exists(dir) && fs::is_directory(dir)) {
				std::error_code ec;
				for (const auto& entry : fs::recursive_directory_iterator(dir, fs::directory_options::skip_permission_denied, ec)) {
					if (entry.is_regular_file() && entry.path().filename().string() == filename) {
						resolvedPath = entry.path().generic_string();
						found = true;
						break;
					}
				}
			}
			if (found) break;
		}
		if (!found) return false;
	}

	_displacementPreviewTex = Texture::Load(resolvedPath.c_str());

	DirectX::TexMetadata metadata = {};
	DirectX::ScratchImage rawImage;
	std::wstring wPath = fs::path(resolvedPath).wstring();
	std::string ext = fs::path(resolvedPath).extension().string();
	std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

	HRESULT hr = E_FAIL;
	if (ext == ".tga") {
		hr = DirectX::LoadFromTGAFile(wPath.c_str(), &metadata, rawImage);
	} else if (ext == ".dds") {
		hr = DirectX::LoadFromDDSFile(wPath.c_str(), DirectX::DDS_FLAGS_NONE, &metadata, rawImage);
	} else {
		hr = DirectX::LoadFromWICFile(wPath.c_str(), DirectX::WIC_FLAGS_NONE, &metadata, rawImage);
	}
	if (FAILED(hr) || rawImage.GetImageCount() == 0) {
		return false;
	}

	const DirectX::Image* srcImg = rawImage.GetImage(0, 0, 0);
	DirectX::ScratchImage convertedImage;
	if (srcImg->format != DXGI_FORMAT_R8G8B8A8_UNORM && srcImg->format != DXGI_FORMAT_R8G8B8A8_UNORM_SRGB) {
		hr = DirectX::Convert(
			*srcImg,
			DXGI_FORMAT_R8G8B8A8_UNORM,
			DirectX::TEX_FILTER_DEFAULT,
			DirectX::TEX_THRESHOLD_DEFAULT,
			convertedImage
		);
		if (FAILED(hr)) return false;
		srcImg = convertedImage.GetImage(0, 0, 0);
	}
	if (!srcImg || !srcImg->pixels) return false;

	_dispWidth  = static_cast<int>(srcImg->width);
	_dispHeight = static_cast<int>(srcImg->height);
	_dispPixels.resize(static_cast<size_t>(_dispWidth) * _dispHeight, 0.0f);

	for (int y = 0; y < _dispHeight; ++y) {
		const uint8_t* row = srcImg->pixels + y * srcImg->rowPitch;
		for (int x = 0; x < _dispWidth; ++x) {
			float r = static_cast<float>(row[x * 4 + 0]) / 255.0f;
			float g = static_cast<float>(row[x * 4 + 1]) / 255.0f;
			float b = static_cast<float>(row[x * 4 + 2]) / 255.0f;
			// Perceived luminance (Rec.709)
			float lum = 0.2126f * r + 0.7152f * g + 0.0722f * b;
			_dispPixels[static_cast<size_t>(y) * _dispWidth + x] = lum;
		}
	}

	return true;
}

float ProceduralMesh::SampleDisplacementBilinear(float u, float v) const {
	if (_dispPixels.empty() || _dispWidth <= 0 || _dispHeight <= 0) {
		return 0.0f;
	}

	// Wrap UV into [0, 1)
	float wu = u - std::floor(u);
	float wv = v - std::floor(v);

	float fx = wu * static_cast<float>(_dispWidth  - 1);
	float fy = wv * static_cast<float>(_dispHeight - 1);

	int x0 = std::clamp(static_cast<int>(std::floor(fx)), 0, _dispWidth  - 1);
	int y0 = std::clamp(static_cast<int>(std::floor(fy)), 0, _dispHeight - 1);
	int x1 = (std::min)(x0 + 1, _dispWidth  - 1);
	int y1 = (std::min)(y0 + 1, _dispHeight - 1);

	float tx = fx - static_cast<float>(x0);
	float ty = fy - static_cast<float>(y0);

	float h00 = _dispPixels[static_cast<size_t>(y0) * _dispWidth + x0];
	float h10 = _dispPixels[static_cast<size_t>(y0) * _dispWidth + x1];
	float h01 = _dispPixels[static_cast<size_t>(y1) * _dispWidth + x0];
	float h11 = _dispPixels[static_cast<size_t>(y1) * _dispWidth + x1];

	float hx0 = h00 + (h10 - h00) * tx;
	float hx1 = h01 + (h11 - h01) * tx;
	return hx0 + (hx1 - hx0) * ty;
}

float ProceduralMesh::SampleDisplacementFiltered(float u, float v) const {
	if (_dispPixels.empty() || _dispWidth <= 0 || _dispHeight <= 0) {
		return 0.0f;
	}

	float su = u * _dispTiling.x + _dispOffset.x;
	float sv = v * _dispTiling.y + _dispOffset.y;

	int radius = std::clamp(_dispSmoothRadius, 0, 4);
	float val = 0.0f;
	if (radius <= 0) {
		val = SampleDisplacementBilinear(su, sv);
	} else {
		float du = 1.0f / static_cast<float>((std::max)(1, _dispWidth));
		float dv = 1.0f / static_cast<float>((std::max)(1, _dispHeight));
		float sum = 0.0f;
		float weightSum = 0.0f;
		for (int dy = -radius; dy <= radius; ++dy) {
			for (int dx = -radius; dx <= radius; ++dx) {
				float w = 1.0f / (1.0f + static_cast<float>(dx * dx + dy * dy));
				sum += SampleDisplacementBilinear(su + dx * du, sv + dy * dv) * w;
				weightSum += w;
			}
		}
		val = (weightSum > 0.0f) ? (sum / weightSum) : 0.0f;
	}

	if (_invertDisplacement) {
		val = 1.0f - val;
	}

	return (val - _displacementMidpoint) * _displacementScale;
}

float ProceduralMesh::EvaluateNoiseHeight(float u, float v) const {
	int octs = std::clamp(_noiseOctaves, 1, 8);
	float freq = (std::max)(0.01f, _noiseFrequency);
	float amp = 1.0f;
	float total = 0.0f;
	float norm = 0.0f;

	if (_noiseType == static_cast<int>(NoiseType::SineWaveRipples)) {
		float cx = (u - 0.5f) * freq * 6.2831853f;
		float cz = (v - 0.5f) * freq * 6.2831853f;
		float r  = std::sqrt(cx * cx + cz * cz);
		float wave = std::sin(cx) * std::cos(cz) * 0.5f + std::sin(r) * 0.5f;
		return wave * _noiseAmplitude;
	}

	for (int i = 0; i < octs; ++i) {
		float n = GradientNoise2D(u * freq, v * freq, _noiseSeed + i * 101);
		if (_noiseType == static_cast<int>(NoiseType::RidgedMountains)) {
			n = 1.0f - std::fabs(n * 2.0f - 1.0f);
			n = n * n;
		}
		total += n * amp;
		norm  += amp;
		amp   *= _noisePersistence;
		freq  *= _noiseLacunarity;
	}

	float h = (norm > 0.0f) ? (total / norm) : 0.0f;

	if (_noiseType == static_cast<int>(NoiseType::TerracedField)) {
		float steps = (std::max)(1.0f, _terraceSteps);
		float scaled = h * steps;
		float fl = std::floor(scaled);
		float fr = scaled - fl;
		// Smooth terrace edge
		float smoothEdge = std::clamp((fr - 0.7f) / 0.3f, 0.0f, 1.0f);
		smoothEdge = smoothEdge * smoothEdge * (3.0f - 2.0f * smoothEdge);
		h = (fl + smoothEdge) / steps;
	}

	// Center around [-0.5, +0.5] for PerlinFBM, or [0, 1] for Ridged/Terrace
	if (_noiseType == static_cast<int>(NoiseType::PerlinFBM)) {
		h = (h - 0.5f) * 2.0f;
	}

	return h * _noiseAmplitude;
}

float ProceduralMesh::EvaluateHeightAt(float normU, float normV, float localX, float localZ) const {
	(void)localX;
	(void)localZ;

	GeneratorMode mode = static_cast<GeneratorMode>(_generatorMode);
	if (mode == GeneratorMode::FlatPlane) {
		return 0.0f;
	}

	float height = 0.0f;
	if (mode == GeneratorMode::DisplacementMap || mode == GeneratorMode::CombinedMapAndNoise) {
		height += SampleDisplacementFiltered(normU, normV);
	}
	if (mode == GeneratorMode::ProceduralNoise || mode == GeneratorMode::CombinedMapAndNoise) {
		height += EvaluateNoiseHeight(normU, normV);
	}

	if (_useIslandFalloff) {
		float dx = (normU - 0.5f) * 2.0f;
		float dz = (normV - 0.5f) * 2.0f;
		float dist = (std::min)(1.0f, std::sqrt(dx * dx + dz * dz));
		float falloff = 1.0f - std::pow(dist, (std::max)(0.1f, _falloffStrength));
		falloff = falloff * falloff * (3.0f - 2.0f * falloff);
		height *= falloff;
	}

	return height;
}

void ProceduralMesh::RecalculateNormals() {
	for (auto& v : _vertices) {
		v.normal = XMFLOAT3(0.0f, 0.0f, 0.0f);
	}

	size_t triCount = _indices.size() / 3;
	for (size_t t = 0; t < triCount; ++t) {
		unsigned int i0 = _indices[t * 3 + 0];
		unsigned int i1 = _indices[t * 3 + 1];
		unsigned int i2 = _indices[t * 3 + 2];
		if (i0 >= _vertices.size() || i1 >= _vertices.size() || i2 >= _vertices.size()) continue;

		KTVECTOR3 p0(_vertices[i0].position.x, _vertices[i0].position.y, _vertices[i0].position.z);
		KTVECTOR3 p1(_vertices[i1].position.x, _vertices[i1].position.y, _vertices[i1].position.z);
		KTVECTOR3 p2(_vertices[i2].position.x, _vertices[i2].position.y, _vertices[i2].position.z);

		// Area-weighted face normal (Cross in LH coordinate system: (p1 - p0) x (p2 - p0))
		KTVECTOR3 fn = Cross(p1 - p0, p2 - p0);
		_vertices[i0].normal.x += fn.x; _vertices[i0].normal.y += fn.y; _vertices[i0].normal.z += fn.z;
		_vertices[i1].normal.x += fn.x; _vertices[i1].normal.y += fn.y; _vertices[i1].normal.z += fn.z;
		_vertices[i2].normal.x += fn.x; _vertices[i2].normal.y += fn.y; _vertices[i2].normal.z += fn.z;
	}

	for (auto& v : _vertices) {
		KTVECTOR3 n(v.normal.x, v.normal.y, v.normal.z);
		if (n.MagnitudeSqr() > 1e-10f) {
			n = n.Normalize();
			v.normal = XMFLOAT3(n.x, n.y, n.z);
		} else {
			v.normal = XMFLOAT3(0.0f, 1.0f, 0.0f);
		}
	}
}

void ProceduralMesh::UploadBuffersToGPU() {
	if (!Renderer::GetDeviceDX12() || _vertices.empty() || _indices.empty()) {
		return;
	}

	RetireBuffers(_vertexBuffer, _indexBuffer);

	_vertexBuffer = Renderer::CreateVertexBuffer(sizeof(Vertex), static_cast<unsigned int>(_vertices.size()));
	if (_vertexBuffer && _vertexBuffer->Resource) {
		void* data = nullptr;
		if (SUCCEEDED(_vertexBuffer->Resource->Map(0, nullptr, &data))) {
			memcpy(data, _vertices.data(), sizeof(Vertex) * _vertices.size());
			_vertexBuffer->Resource->Unmap(0, nullptr);
		}
	}

	_indexCount = static_cast<unsigned int>(_indices.size());
	_indexBuffer = Renderer::CreateIndexBuffer(_indexCount);
	if (_indexBuffer && _indexBuffer->Resource) {
		void* data = nullptr;
		if (SUCCEEDED(_indexBuffer->Resource->Map(0, nullptr, &data))) {
			memcpy(data, _indices.data(), sizeof(unsigned int) * _indices.size());
			_indexBuffer->Resource->Unmap(0, nullptr);
		}
	}
}

void ProceduralMesh::RebuildMesh() {
	_segmentsX = std::clamp(_segmentsX, 1, 256);
	_segmentsZ = std::clamp(_segmentsZ, 1, 256);
	_width     = (std::max)(0.05f, _width);
	_depth     = (std::max)(0.05f, _depth);

	if ((_generatorMode == static_cast<int>(GeneratorMode::DisplacementMap) ||
	     _generatorMode == static_cast<int>(GeneratorMode::CombinedMapAndNoise)) &&
	    !_displacementMapPath.empty() && _dispPixels.empty()) {
		LoadDisplacementMapCPU(_displacementMapPath);
	}

	int vertsX = _segmentsX + 1;
	int vertsZ = _segmentsZ + 1;

	_vertices.resize(static_cast<size_t>(vertsX) * vertsZ);
	_indices.clear();
	_indices.reserve(static_cast<size_t>(_segmentsX) * _segmentsZ * 6);

	float halfW = _width * 0.5f;
	float halfD = _depth * 0.5f;

	for (int iz = 0; iz < vertsZ; ++iz) {
		float v = static_cast<float>(iz) / static_cast<float>(_segmentsZ); // [0..1] from +Z to -Z
		float localZ = halfD - v * _depth;
		for (int ix = 0; ix < vertsX; ++ix) {
			float u = static_cast<float>(ix) / static_cast<float>(_segmentsX); // [0..1] from -X to +X
			float localX = -halfW + u * _width;

			float localY = EvaluateHeightAt(u, v, localX, localZ);

			Vertex& vert = _vertices[static_cast<size_t>(iz) * vertsX + ix];
			vert.position = XMFLOAT3(localX, localY, localZ);
			vert.normal   = XMFLOAT3(0.0f, 1.0f, 0.0f);
			vert.color    = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
			vert.uv       = XMFLOAT2(u, v);
		}
	}

	for (int iz = 0; iz < _segmentsZ; ++iz) {
		for (int ix = 0; ix < _segmentsX; ++ix) {
			unsigned int topLeft     = static_cast<unsigned int>(iz * vertsX + ix);
			unsigned int topRight    = topLeft + 1;
			unsigned int bottomLeft  = static_cast<unsigned int>((iz + 1) * vertsX + ix);
			unsigned int bottomRight = bottomLeft + 1;

			// Clockwise winding looking down (-Y) onto the +Y top surface in Left-Handed DirectX
			_indices.push_back(topLeft);
			_indices.push_back(topRight);
			_indices.push_back(bottomLeft);

			_indices.push_back(bottomLeft);
			_indices.push_back(topRight);
			_indices.push_back(bottomRight);
		}
	}

	RecalculateNormals();
	OnPostGenerateVertices(_vertices, vertsX, vertsZ);
	UploadBuffersToGPU();

	_meshRevision++;

	// Notify attached ColliderMesh on the same GameObject if present
	if (_owner) {
		ColliderMesh* colMesh = _owner->GetComponent<ColliderMesh>();
		if (colMesh) {
			colMesh->RebuildMesh();
			colMesh->SyncTransform();
		}
	}
}

float ProceduralMesh::SampleHeightAtLocalXZ(float localX, float localZ) const {
	if (_vertices.empty() || _segmentsX < 1 || _segmentsZ < 1) return 0.0f;

	float halfW = _width * 0.5f;
	float halfD = _depth * 0.5f;
	float u = std::clamp((localX + halfW) / _width, 0.0f, 1.0f);
	float v = std::clamp((halfD - localZ) / _depth, 0.0f, 1.0f);

	int vertsX = _segmentsX + 1;
	float fx = u * static_cast<float>(_segmentsX);
	float fz = v * static_cast<float>(_segmentsZ);

	int ix0 = std::clamp(static_cast<int>(std::floor(fx)), 0, _segmentsX - 1);
	int iz0 = std::clamp(static_cast<int>(std::floor(fz)), 0, _segmentsZ - 1);
	int ix1 = ix0 + 1;
	int iz1 = iz0 + 1;

	float tx = fx - static_cast<float>(ix0);
	float tz = fz - static_cast<float>(iz0);

	float h00 = _vertices[static_cast<size_t>(iz0) * vertsX + ix0].position.y;
	float h10 = _vertices[static_cast<size_t>(iz0) * vertsX + ix1].position.y;
	float h01 = _vertices[static_cast<size_t>(iz1) * vertsX + ix0].position.y;
	float h11 = _vertices[static_cast<size_t>(iz1) * vertsX + ix1].position.y;

	float hx0 = h00 + (h10 - h00) * tx;
	float hx1 = h01 + (h11 - h01) * tx;
	return hx0 + (hx1 - hx0) * tz;
}

void ProceduralMesh::Render() const {
	if (!_owner || !_vertexBuffer || !_indexBuffer || _indexCount == 0) return;

	auto cmdList = Renderer::GetCommandListDX12();
	if (!cmdList) return;

	XMMATRIX worldMatrix = _owner->GetWorldMatrix();

	// 0. Shadow Pass
	if (Renderer::IsShadowPass()) {
		ID3D12PipelineState* shadowPSO = Renderer::GetShadowPipelineState();
		if (!shadowPSO) return;
		cmdList->SetPipelineState(shadowPSO);

		D3D12_VERTEX_BUFFER_VIEW vbView = {};
		vbView.BufferLocation = _vertexBuffer->Resource->GetGPUVirtualAddress();
		vbView.StrideInBytes  = _vertexBuffer->Stride;
		vbView.SizeInBytes    = _vertexBuffer->Stride * _vertexBuffer->Size;
		cmdList->IASetVertexBuffers(0, 1, &vbView);

		D3D12_INDEX_BUFFER_VIEW ibView = {};
		ibView.BufferLocation = _indexBuffer->Resource->GetGPUVirtualAddress();
		ibView.SizeInBytes    = sizeof(unsigned int) * _indexBuffer->Size;
		ibView.Format         = DXGI_FORMAT_R32_UINT;
		cmdList->IASetIndexBuffer(&ibView);

		cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		Renderer::SetWorldMatrix(worldMatrix);
		cmdList->DrawIndexedInstanced(_indexCount, 1, 0, 0, 0);
		Profiler::RecordDrawCall(static_cast<int>(_vertices.size()), static_cast<int>(_indexCount));
		return;
	}

	int blendMode = 0;
	if (Renderer::IsGeometryPass() && blendMode != 0) return;
	if (!Renderer::IsGeometryPass() && blendMode == 0) return;

	D3D12_VERTEX_BUFFER_VIEW vbView = {};
	vbView.BufferLocation = _vertexBuffer->Resource->GetGPUVirtualAddress();
	vbView.StrideInBytes  = _vertexBuffer->Stride;
	vbView.SizeInBytes    = _vertexBuffer->Stride * _vertexBuffer->Size;
	cmdList->IASetVertexBuffers(0, 1, &vbView);

	D3D12_INDEX_BUFFER_VIEW ibView = {};
	ibView.BufferLocation = _indexBuffer->Resource->GetGPUVirtualAddress();
	ibView.SizeInBytes    = sizeof(unsigned int) * _indexBuffer->Size;
	ibView.Format         = DXGI_FORMAT_R32_UINT;
	cmdList->IASetIndexBuffer(&ibView);

	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	auto shaderComp = _owner->GetComponent<Shader>();
	{
		std::string vsId = "VertexDirectionalLightingVS";
		std::string psId = "VertexDirectionalLightingPS";
		if (shaderComp) {
			vsId = shaderComp->GetVertexShaderID();
			psId = shaderComp->GetPixelShaderID();
		}
		ID3D12PipelineState* pso = ShaderManager::Instance().GetPipelineState(
			vsId, psId, 0,
			Renderer::GetCullModeDX12(),
			Renderer::GetDepthEnableDX12(),
			Renderer::GetDepthWriteDX12(),
			D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE
		);
		if (!pso) return;
		cmdList->SetPipelineState(pso);
	}

	Renderer::SetWorldMatrix(worldMatrix);

	MATERIAL material = {};
	material.Diffuse  = { 1.0f, 1.0f, 1.0f, 1.0f };
	material.Ambient  = { 1.0f, 1.0f, 0.0f, 0.0f };
	material.Specular = { 0.0f, 0.0f, 1.0f, 1337.0f };

	const TEXTURE* tex = shaderComp ? shaderComp->GetTexture() : nullptr;
	material.TextureEnable = (tex != nullptr);

	if (shaderComp) {
		XMFLOAT4 scColor = shaderComp->GetBaseColor();
		if (tex && scColor.x == 0.0f && scColor.y == 0.0f && scColor.z == 0.0f) {
			scColor = { 1.0f, 1.0f, 1.0f, 1.0f };
		}
		material.BaseColor = scColor;
		shaderComp->FillMaterialStruct(material);
	} else {
		material.BaseColor      = { 0.78f, 0.80f, 0.82f, 1.0f };
		material.EmissionColor  = { 0.0f, 0.0f, 0.0f, 0.0f };
		material.Metallic       = 0.0f;
		material.SpecularPbr    = 0.5f;
		material.Roughness      = 0.65f;
		material.NormalWeight   = 1.0f;
		material.ShadingModelID = 0;
		material.HasNormalMap   = 0;
		material.HasMetallicMap = 0;
		material.HasRoughnessMap = 0;
	}

	Renderer::SetConstant(3, &material, sizeof(material));

	if (tex) {
		Renderer::SetTexture(6, tex);
	}
	if (shaderComp && shaderComp->GetNormalMap()) {
		Renderer::SetTexture(7, shaderComp->GetNormalMap());
	}
	if (shaderComp) {
		Renderer::SetTexture(8, shaderComp->GetARMMap());
	} else {
		Renderer::SetTexture(8, nullptr);
	}

	cmdList->DrawIndexedInstanced(_indexCount, 1, 0, 0, 0);
	Profiler::RecordDrawCall(static_cast<int>(_vertices.size()), static_cast<int>(_indexCount));
}

void ProceduralMesh::ShowUI() {
	bool dirty = false;

	ImGui::TextColored(ImVec4(0.45f, 0.88f, 1.0f, 1.0f), "Procedural Mesh / Field Generator");
	ImGui::Text("Vertices: %d | Triangles: %d", (int)_vertices.size(), (int)(_indices.size() / 3));

	// Quick Presets
	ImGui::Text("Quick Presets:");
	if (ImGui::SmallButton("Flat Plane")) {
		_generatorMode = static_cast<int>(GeneratorMode::FlatPlane);
		dirty = true;
	}
	ImGui::SameLine();
	if (ImGui::SmallButton("Perlin Hills")) {
		_generatorMode = static_cast<int>(GeneratorMode::ProceduralNoise);
		_noiseType = static_cast<int>(NoiseType::PerlinFBM);
		_noiseAmplitude = 0.35f;
		_noiseFrequency = 3.0f;
		dirty = true;
	}
	ImGui::SameLine();
	if (ImGui::SmallButton("Ridged Peaks")) {
		_generatorMode = static_cast<int>(GeneratorMode::ProceduralNoise);
		_noiseType = static_cast<int>(NoiseType::RidgedMountains);
		_noiseAmplitude = 0.55f;
		_noiseFrequency = 2.5f;
		dirty = true;
	}
	ImGui::SameLine();
	if (ImGui::SmallButton("Terraced")) {
		_generatorMode = static_cast<int>(GeneratorMode::ProceduralNoise);
		_noiseType = static_cast<int>(NoiseType::TerracedField);
		_noiseAmplitude = 0.45f;
		_terraceSteps = 6.0f;
		dirty = true;
	}

	ImGui::Separator();
	ImGui::Text("Base Plane Grid:");
	float dims[2] = { _width, _depth };
	if (ImGui::DragFloat2("Size (Width, Depth)", dims, 0.05f, 0.1f, 500.0f, "%.2f")) {
		_width = (std::max)(0.1f, dims[0]);
		_depth = (std::max)(0.1f, dims[1]);
		dirty = true;
	}
	int segs[2] = { _segmentsX, _segmentsZ };
	if (ImGui::SliderInt2("Subdivisions (X, Z)", segs, 1, 200)) {
		_segmentsX = std::clamp(segs[0], 1, 256);
		_segmentsZ = std::clamp(segs[1], 1, 256);
		dirty = true;
	}

	ImGui::Separator();
	const char* genModes[] = {
		"Flat Plane (Default)",
		"Displacement Map (PNG / Texture)",
		"Procedural Noise (Perlin / Ridged / Terrace)",
		"Combined (PNG Displacement + Procedural Noise)"
	};
	if (ImGui::Combo("Height Mode", &_generatorMode, genModes, IM_ARRAYSIZE(genModes))) {
		dirty = true;
	}

	bool showDispUI  = (_generatorMode == 1 || _generatorMode == 3);
	bool showNoiseUI = (_generatorMode == 2 || _generatorMode == 3);

	if (showDispUI) {
		ImGui::Separator();
		ImGui::TextColored(ImVec4(0.95f, 0.82f, 0.35f, 1.0f), "Displacement Map (PNG Luminance -> Height):");

		std::string currentLabel = _displacementMapPath.empty()
			? "(None)"
			: std::filesystem::path(_displacementMapPath).filename().string();

		if (ImGui::BeginCombo("Displacement PNG", currentLabel.c_str())) {
			if (ImGui::Selectable("(None)", _displacementMapPath.empty())) {
				SetDisplacementMapPath("");
			}
			auto texFiles = GetProjectTextureFiles();
			for (const auto& f : texFiles) {
				bool isSelected = (_displacementMapPath == f);
				std::string itemLabel = std::filesystem::path(f).filename().string() + " (" + f + ")";
				if (ImGui::Selectable(itemLabel.c_str(), isSelected)) {
					SetDisplacementMapPath(f);
				}
				if (isSelected) ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}

		if (_displacementPreviewTex) {
			void* srv = Renderer::GetTextureSRV(_displacementPreviewTex);
			if (srv) {
				ImGui::Image(srv, ImVec2(64.0f, 64.0f));
				ImGui::SameLine();
				ImGui::BeginGroup();
				ImGui::Text("Loaded: %d x %d px", _dispWidth, _dispHeight);
				if (ImGui::SmallButton("Reload PNG")) {
					LoadDisplacementMapCPU(_displacementMapPath);
					dirty = true;
				}
				ImGui::EndGroup();
			}
		}

		if (ImGui::DragFloat("Displacement Scale", &_displacementScale, 0.01f, -50.0f, 50.0f, "%.3f")) dirty = true;
		if (ImGui::SliderFloat("Midpoint Offset", &_displacementMidpoint, 0.0f, 1.0f, "%.2f")) dirty = true;
		if (ImGui::SliderInt("Smooth Filter Radius", &_dispSmoothRadius, 0, 4)) dirty = true;
		if (ImGui::Checkbox("Invert Luminance (Black=High)", &_invertDisplacement)) dirty = true;

		float tiling[2] = { _dispTiling.x, _dispTiling.y };
		if (ImGui::DragFloat2("Map UV Tiling", tiling, 0.02f, 0.01f, 50.0f, "%.2f")) {
			_dispTiling = { tiling[0], tiling[1] };
			dirty = true;
		}
		float offset[2] = { _dispOffset.x, _dispOffset.y };
		if (ImGui::DragFloat2("Map UV Offset", offset, 0.01f, -20.0f, 20.0f, "%.2f")) {
			_dispOffset = { offset[0], offset[1] };
			dirty = true;
		}

		if (ImGui::SmallButton("Open Procedural Texture Editor...")) {
			Manager::GetShowProceduralTextureEditor() = true;
		}
	}

	if (showNoiseUI) {
		ImGui::Separator();
		ImGui::TextColored(ImVec4(0.55f, 0.92f, 0.62f, 1.0f), "Procedural Field Noise:");
		const char* noiseTypes[] = {
			"Perlin FBM Hills",
			"Ridged Mountains",
			"Terraced Canyon / Steps",
			"Sine Wave Ripples"
		};
		if (ImGui::Combo("Noise Type", &_noiseType, noiseTypes, IM_ARRAYSIZE(noiseTypes))) dirty = true;
		if (ImGui::DragFloat("Noise Amplitude", &_noiseAmplitude, 0.01f, -50.0f, 50.0f, "%.3f")) dirty = true;
		if (ImGui::DragFloat("Noise Frequency", &_noiseFrequency, 0.05f, 0.05f, 50.0f, "%.2f")) dirty = true;
		if (_noiseType != static_cast<int>(NoiseType::SineWaveRipples)) {
			if (ImGui::SliderInt("Octaves", &_noiseOctaves, 1, 8)) dirty = true;
			if (ImGui::SliderFloat("Persistence", &_noisePersistence, 0.1f, 0.95f, "%.2f")) dirty = true;
			if (ImGui::SliderFloat("Lacunarity", &_noiseLacunarity, 1.1f, 4.0f, "%.2f")) dirty = true;
			if (ImGui::DragInt("Noise Seed", &_noiseSeed, 1.0f, 1, 999999)) dirty = true;
		}
		if (_noiseType == static_cast<int>(NoiseType::TerracedField)) {
			if (ImGui::SliderFloat("Terrace Steps", &_terraceSteps, 2.0f, 24.0f, "%.1f")) dirty = true;
		}
	}

	if (_generatorMode != 0) {
		ImGui::Separator();
		if (ImGui::Checkbox("Apply Island Edge Falloff", &_useIslandFalloff)) dirty = true;
		if (_useIslandFalloff) {
			if (ImGui::SliderFloat("Falloff Curve", &_falloffStrength, 0.2f, 5.0f, "%.2f")) dirty = true;
		}
	}

	ImGui::Separator();
	if (ImGui::Button("Rebuild Mesh & Collider")) {
		dirty = true;
	}

	if (dirty) {
		RebuildMesh();
	}
}
