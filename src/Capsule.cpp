#include "Capsule.h"
#include "GameObject.h"
#include "ktvector.hpp"
#include "Texture.h"
#include <imgui.h>
#include "ShaderManager.h"
#include "Shader.h"

void Capsule::CreateCapsuleMesh(float height, float radius, int latitudes, int longitudes, std::vector<Vertex>& vertices, std::vector<UINT>& indices){
	float cylinderHeight = (std::max)(0.0f, height - 2.0f * radius);
	float yOffset = cylinderHeight * 0.5f;

	int rings = latitudes + 1;
	for (int i = 0; i <= rings; ++i) {
		float v = (float)i / rings;
		float phi = 0.0f;
		float currentYOffset = 0.0f;

		if (i <= latitudes / 2) {
			phi = ((float)i / latitudes) * XM_PI;
			currentYOffset = yOffset;
		}
		else {
			phi = ((float)(i - 1) / latitudes) * XM_PI;
			currentYOffset = -yOffset;
		}
		for (int j = 0; j <= longitudes; ++j) {
			float u = (float)j / longitudes;
			float theta = u * XM_2PI;

			float x = radius * sin(phi) * cos(theta);
			float y = radius * cos(phi);
			float z = radius * sin(phi) * sin(theta);

			Vertex vertex;
			vertex.position = { x, y + currentYOffset, z };
			KTVECTOR3 normal = KTVECTOR3(x, y, z);
			normal = normal.Normalize();
			vertex.normal = { normal.x, normal.y, normal.z };
			vertex.uv = XMFLOAT2(u, v);
			vertex.color = { 1.0f, 1.0f, 1.0f, 1.0f };
			vertices.push_back(vertex);
		}
	}

	for (int i = 0; i < rings; ++i) {
		for (int j = 0; j < longitudes; ++j) {
			int first = (i * (longitudes + 1)) + j;
			int second = first + longitudes + 1;

			indices.push_back(first);
			indices.push_back(second);
			indices.push_back(first + 1);

			indices.push_back(second);
			indices.push_back(second + 1);
			indices.push_back(first + 1);
		}
	}
}

void Capsule::RebuildBuffers(){
	std::vector<Vertex> vertices;
	std::vector<UINT> indices;
	CreateCapsuleMesh(_height, _radius, _latitudes, _longitudes, vertices, indices);
	_indexCount = indices.size();

	_vertexBuffer = Renderer::CreateVertexBuffer(sizeof(Vertex), vertices.size());
	void* data = nullptr;
	HRESULT hr = _vertexBuffer->Resource->Map(0, nullptr, &data);
	if (SUCCEEDED(hr)) {
		memcpy(data, vertices.data(), sizeof(Vertex) * vertices.size());
		_vertexBuffer->Resource->Unmap(0, nullptr);
	}

	_indexBuffer = Renderer::CreateIndexBuffer(indices.size());
	hr = _indexBuffer->Resource->Map(0, nullptr, &data);
	if (SUCCEEDED(hr)) {
		memcpy(data, indices.data(), sizeof(UINT) * indices.size());
		_indexBuffer->Resource->Unmap(0, nullptr);
	}
}

void Capsule::UpdateBuffers(){
	std::vector<Vertex> vertices;
	std::vector<UINT> indices;
	CreateCapsuleMesh(_height, _radius, _latitudes, _longitudes, vertices, indices);

	void* data = nullptr;
	HRESULT hr = _vertexBuffer->Resource->Map(0, nullptr, &data);
	if (SUCCEEDED(hr)) {
		memcpy(data, vertices.data(), sizeof(Vertex) * vertices.size());
		_vertexBuffer->Resource->Unmap(0, nullptr);
	}
}

void Capsule::Awake(){
	if (_owner && !_owner->GetComponent<Shader>()) {
		_owner->AddComponent<Shader>();
	}
	RebuildBuffers();
	_texture = Texture::Load("asset/texture/default.png");
}

void Capsule::Render() const{
	int blendMode = 0;
	if (Renderer::IsGeometryPass() && blendMode != 0) return;
	if (!Renderer::IsGeometryPass() && blendMode == 0) return;

	auto cmdList = Renderer::GetCommandListDX12();
	if (!cmdList) return;

	D3D12_VERTEX_BUFFER_VIEW vbView = {};
	vbView.BufferLocation = _vertexBuffer->Resource->GetGPUVirtualAddress();
	vbView.StrideInBytes = _vertexBuffer->Stride;
	vbView.SizeInBytes = _vertexBuffer->Stride * _vertexBuffer->Size;
	cmdList->IASetVertexBuffers(0, 1, &vbView);

	D3D12_INDEX_BUFFER_VIEW ibView = {};
	ibView.BufferLocation = _indexBuffer->Resource->GetGPUVirtualAddress();
	ibView.SizeInBytes = sizeof(unsigned int) * _indexBuffer->Size;
	ibView.Format = DXGI_FORMAT_R32_UINT;
	cmdList->IASetIndexBuffer(&ibView);

	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	// PSOバインド
	{
		std::string vsId = "VertexDirectionalLightingVS";
		std::string psId = "VertexDirectionalLightingPS";
		auto shaderComp = _owner->GetComponent<Shader>();
		if (shaderComp) {
			vsId = shaderComp->GetVertexShaderID();
			psId = shaderComp->GetPixelShaderID();
		}
		ID3D12PipelineState* pso = ShaderManager::Instance().GetPipelineState(vsId, psId, 0, Renderer::GetCullModeDX12(), Renderer::GetDepthEnableDX12(), Renderer::GetDepthWriteDX12(), D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
		if (pso == nullptr) return;
		cmdList->SetPipelineState(pso);
	}


	XMMATRIX translation = XMMatrixTranslation(_owner->_transform._position.x, _owner->_transform._position.y, _owner->_transform._position.z);
	XMFLOAT4 q = XMFLOAT4(_owner->_transform._quaternion.x, _owner->_transform._quaternion.y, _owner->_transform._quaternion.z, _owner->_transform._quaternion.w);
	XMMATRIX rotation = XMMatrixRotationQuaternion(XMLoadFloat4(&q));
	XMMATRIX scaling = XMMatrixScaling(_owner->_transform._scale.x, _owner->_transform._scale.y, _owner->_transform._scale.z);
	XMMATRIX worldMatrix = scaling * rotation * translation;

	Renderer::SetWorldMatrix(worldMatrix);

	MATERIAL material = {};
	material.Diffuse = { 1.0f, 1.0f, 1.0f, 1.0f };

	auto shaderComp = _owner->GetComponent<Shader>();
	const TEXTURE* tex = (shaderComp && shaderComp->GetTexture()) ? shaderComp->GetTexture() : _texture;
	material.TextureEnable = (tex != nullptr);

	if (shaderComp) {
		XMFLOAT4 scColor = shaderComp->GetBaseColor();
		if (tex && scColor.x == 0.0f && scColor.y == 0.0f && scColor.z == 0.0f) {
			scColor = { 1.0f, 1.0f, 1.0f, 1.0f };
		}
		material.BaseColor = scColor;
		material.EmissionColor = shaderComp->GetEmissionColor();
		material.Metallic = shaderComp->GetMetallic();
		material.SpecularPbr = shaderComp->GetSpecular();
		material.Roughness = shaderComp->GetRoughness();
		material.NormalWeight = shaderComp->GetNormalWeight();
		material.ShadingModelID = shaderComp->GetShadingModelID();
		material.FlipU = shaderComp->GetFlipU() ? 1 : 0;
		material.FlipV = shaderComp->GetFlipV() ? 1 : 0;
		material.HasNormalMap = shaderComp->GetNormalMap() ? 1 : 0;
		material.HasMetallicMap = shaderComp->GetMetallicMap() ? 1 : 0;
		material.HasRoughnessMap = shaderComp->GetRoughnessMap() ? 1 : 0;
	} else {
		material.BaseColor = { 1.0f, 1.0f, 1.0f, 1.0f };
		material.EmissionColor = { 0.0f, 0.0f, 0.0f, 0.0f };
		material.Metallic = 0.0f;
		material.SpecularPbr = 0.5f;
		material.Roughness = 0.5f;
		material.NormalWeight = 1.0f;
		material.ShadingModelID = 0;
		material.HasNormalMap = 0;
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
	if (shaderComp && shaderComp->GetMetallicMap()) {
		Renderer::SetTexture(8, shaderComp->GetMetallicMap());
	}
	if (shaderComp && shaderComp->GetRoughnessMap()) {
		Renderer::SetTexture(9, shaderComp->GetRoughnessMap());
	}

	cmdList->DrawIndexedInstanced(_indexCount, 1, 0, 0, 0);
}

void Capsule::ShowUI(){
	bool shapeChanged = false;
	bool topologyChanged = false;
	if (ImGui::InputFloat("Height", &_height, 0.1f, 1.0f, "%.3f")) {
		if (_height < 0.0f) _height = 0.0f;
		shapeChanged = true;
	}
	if (ImGui::InputFloat("Radius", &_radius, 0.1f, 1.0f, "%.3f")) {
		if (_radius < 0.01f) _radius = 0.01f;
		shapeChanged = true;
	}

	if (ImGui::InputInt("Latitudes", &_latitudes, 1, 5)) {
		if (_latitudes < 4) _latitudes = 4;
		topologyChanged = true;
	}

	if (ImGui::InputInt("Longitudes", &_longitudes, 1, 5)) {
		if (_longitudes < 3) _longitudes = 3;
		topologyChanged = true;
	}

	if (topologyChanged) {
		RebuildBuffers();
	}
	else if (shapeChanged) {
		UpdateBuffers();
	}
}
