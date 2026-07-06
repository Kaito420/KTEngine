#include "Sphere.h"
#include "GameObject.h"
#include <imgui.h>
#include "Texture.h"
#include "ShaderManager.h"
#include "Shader.h"

void Sphere::CreateSphereMesh(float radius, int sliceCount, int stackCount, std::vector<Vertex>& vertices, std::vector<UINT>& indices){
	vertices.clear();
	indices.clear();

	// gbv
	vertices.push_back({ XMFLOAT3(0.0f, radius, 0.0f),
						 XMFLOAT3(0.0f, 1.0f, 0.0f),
						 XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f),
						 XMFLOAT2(0.5f, 0.0f)
		});

	float phiStep = XM_PI / stackCount;
	float thetaStep = 2.0f * XM_PI / sliceCount;

	for (UINT i = 1; i <= stackCount - 1; ++i)
	{
		float phi = i * phiStep;

		for (UINT j = 0; j <= sliceCount; ++j)
		{
			float theta = j * thetaStep;

			XMFLOAT3 pos(
				radius * sinf(phi) * cosf(theta),
				radius * cosf(phi),
				radius * sinf(phi) * sinf(theta));

			XMFLOAT3 normal;
			XMStoreFloat3(&normal, XMVector3Normalize(XMLoadFloat3(&pos)));

			XMFLOAT4 diffuse = { 1.0f, 1.0f, 1.0f, 1.0f };

			XMFLOAT2 texCoord(theta / XM_2PI, phi / XM_PI);

			vertices.push_back({ pos, normal, diffuse, texCoord });
		}
	}

	// {g
	vertices.push_back({ XMFLOAT3(0.0f, -radius, 0.0f),
						 XMFLOAT3(0.0f, -1.0f, 0.0f),
						 XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f),
						 XMFLOAT2(0.5f, 1.0f) });

	// top stack (top pole to first ring)
	for (UINT i = 0; i < sliceCount; ++i)
	{
		indices.push_back(0); // top pole index
		indices.push_back((i + 1) % sliceCount + 1);
		indices.push_back(i + 1);
	}

	// middle stacks
	UINT baseIndex = 1;
	UINT ringVertexCount = sliceCount + 1;

	for (UINT i = 0; i < stackCount - 2; ++i)
	{
		for (UINT j = 0; j < sliceCount; ++j)
		{
			indices.push_back(baseIndex + i * ringVertexCount + j);
			indices.push_back(baseIndex + i * ringVertexCount + j + 1);
			indices.push_back(baseIndex + (i + 1) * ringVertexCount + j);

			indices.push_back(baseIndex + (i + 1) * ringVertexCount + j);
			indices.push_back(baseIndex + i * ringVertexCount + j + 1);
			indices.push_back(baseIndex + (i + 1) * ringVertexCount + j + 1);
		}
	}

	// bottom stack
	UINT southPoleIndex = (UINT)vertices.size() - 1;
	baseIndex = southPoleIndex - ringVertexCount;

	for (UINT i = 0; i < sliceCount; ++i)
	{
		indices.push_back(southPoleIndex);
		indices.push_back(baseIndex + i);
		indices.push_back(baseIndex + i + 1);
	}
}

void Sphere::RebuildBuffers() {
	std::vector<Vertex> vertices;
	std::vector<UINT> indices;
	CreateSphereMesh(_radius, _stackCount, _sliceCount, vertices, indices);
	_indexCount = (int)indices.size();

	_vertexBuffer = Renderer::CreateVertexBuffer(sizeof(Vertex), (UINT)vertices.size());
	if (_vertexBuffer && _vertexBuffer->Resource) {
		void* data = nullptr;
		HRESULT hr = _vertexBuffer->Resource->Map(0, nullptr, &data);
		if (SUCCEEDED(hr)) {
			memcpy(data, vertices.data(), sizeof(Vertex) * vertices.size());
			_vertexBuffer->Resource->Unmap(0, nullptr);
		}
	}

	_indexBuffer = Renderer::CreateIndexBuffer((UINT)indices.size());
	if (_indexBuffer && _indexBuffer->Resource) {
		void* data = nullptr;
		HRESULT hr = _indexBuffer->Resource->Map(0, nullptr, &data);
		if (SUCCEEDED(hr)) {
			memcpy(data, indices.data(), sizeof(UINT) * indices.size());
			_indexBuffer->Resource->Unmap(0, nullptr);
		}
	}
}

void Sphere::UpdateBuffers() {
	std::vector<Vertex> vertices;
	std::vector<UINT> indices;
	CreateSphereMesh(_radius, _stackCount, _sliceCount, vertices, indices);

	if (_vertexBuffer && _vertexBuffer->Resource) {
		void* data = nullptr;
		HRESULT hr = _vertexBuffer->Resource->Map(0, nullptr, &data);
		if (SUCCEEDED(hr)) {
			memcpy(data, vertices.data(), sizeof(Vertex) * vertices.size());
			_vertexBuffer->Resource->Unmap(0, nullptr);
		}
	}
}

void Sphere::Awake() {
	if (_owner && !_owner->GetComponent<Shader>()) {
		_owner->AddComponent<Shader>();
	}
	RebuildBuffers();
	_texture = Texture::Load("asset/texture/default.png");
}

void Sphere::Render()const {
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
	} else {
		material.BaseColor = { 1.0f, 1.0f, 1.0f, 1.0f };
		material.EmissionColor = { 0.0f, 0.0f, 0.0f, 0.0f };
		material.Metallic = 0.0f;
		material.SpecularPbr = 0.5f;
		material.Roughness = 0.5f;
		material.NormalWeight = 1.0f;
		material.ShadingModelID = 0;
	}

	Renderer::SetConstant(3, &material, sizeof(material));

	if (tex) {
		Renderer::SetTexture(6, tex);
	}

	cmdList->DrawIndexedInstanced(_indexCount, 1, 0, 0, 0);
}

void Sphere::ShowUI() {
	bool shapeChanged = false;
	bool topologyChanged = false;

	if (ImGui::InputFloat("Radius", &_radius, 0.1f, 1.0f, "%.3f")) {
		if (_radius < 0.01f) _radius = 0.01f;
		shapeChanged = true;
	}

	if (ImGui::InputInt("Latitudes", &_stackCount, 1, 5)) {
		if (_stackCount < 4) _stackCount = 4;
		topologyChanged = true;
	}

	if (ImGui::InputInt("Longitudes", &_sliceCount, 1, 5)) {
		if (_sliceCount < 4) _sliceCount = 4;
		topologyChanged = true;
	}

	if (topologyChanged) {
		RebuildBuffers();
	}
	else if (shapeChanged) {
		UpdateBuffers();
	}
}
