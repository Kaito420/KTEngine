#include "Cube.h"
#include "GameObject.h"
#include "ShaderManager.h"
#include "Shader.h"

void Cube::Awake() {
	_vertexBuffer = Renderer::CreateVertexBuffer(sizeof(Vertex), 24);

	Vertex v[] = {
		// O̖
		{ { -0.5f,  0.5f, -0.5f },{0.0f, 0.0f, -1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f} },
		{ {  0.5f,  0.5f, -0.5f },{0.0f, 0.0f, -1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {1.0f, 0.0f} },
		{ { -0.5f, -0.5f, -0.5f },{0.0f, 0.0f, -1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 1.0f} },
		{ {  0.5f, -0.5f, -0.5f },{0.0f, 0.0f, -1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {1.0f, 1.0f} },

		// E̖
		{ { 0.5f,  0.5f, -0.5f },{1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f}, {0.0f, 0.0f} },
		{ { 0.5f,  0.5f,  0.5f },{1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f}, {1.0f, 0.0f} },
		{ { 0.5f, -0.5f, -0.5f },{1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f}, {0.0f, 1.0f} },
		{ { 0.5f, -0.5f,  0.5f },{1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f}, {1.0f, 1.0f} },

		// ̖
		{ { 0.5f,  0.5f, -0.5f },{0.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 1.0f, 1.0f}, {0.0f, 0.0f} },
		{ { -0.5f, 0.5f, -0.5f },{0.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 1.0f, 1.0f}, {1.0f, 0.0f} },
		{ { -0.5f, 0.5f,  0.5f },{0.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 1.0f, 1.0f}, {0.0f, 1.0f} },
		{ { 0.5f,  0.5f,  0.5f },{0.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 1.0f, 1.0f}, {1.0f, 1.0f} },

		// ̖
		{ { -0.5f,  0.5f, 0.5f },{0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f, 1.0f}, {0.0f, 0.0f} },
		{ { -0.5f, -0.5f, 0.5f },{0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f, 1.0f}, {1.0f, 0.0f} },
		{ {  0.5f,  0.5f, 0.5f },{0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f, 1.0f}, {0.0f, 1.0f} },
		{ {  0.5f, -0.5f, 0.5f },{0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f, 1.0f}, {1.0f, 1.0f} },

		// ̖
		{ { -0.5f,  0.5f, -0.5f },{-1.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 0.0f, 1.0f}, {0.0f, 0.0f} },
		{ { -0.5f, -0.5f, -0.5f },{-1.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 0.0f, 1.0f}, {1.0f, 0.0f} },
		{ { -0.5f,  0.5f,  0.5f },{-1.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 0.0f, 1.0f}, {0.0f, 1.0f} },
		{ { -0.5f, -0.5f,  0.5f },{-1.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 0.0f, 1.0f}, {1.0f, 1.0f} },

		// ̖
		{ {  0.5f, -0.5f, -0.5f },{0.0f, -1.0f, 0.0f}, {0.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 0.0f} },
		{ { -0.5f, -0.5f,  0.5f },{0.0f, -1.0f, 0.0f}, {0.0f, 1.0f, 1.0f, 1.0f}, {1.0f, 0.0f} },
		{ { -0.5f, -0.5f, -0.5f },{0.0f, -1.0f, 0.0f}, {0.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 1.0f} },
		{ {  0.5f, -0.5f,  0.5f },{0.0f, -1.0f, 0.0f}, {0.0f, 1.0f, 1.0f, 1.0f}, {1.0f, 1.0f} }
	};

	void* data = nullptr;
	HRESULT hr = _vertexBuffer->Resource->Map(0, nullptr, &data);
	if (SUCCEEDED(hr)) {
		memcpy(data, v, sizeof(v));
		_vertexBuffer->Resource->Unmap(0, nullptr);
	}

	_indexBuffer = Renderer::CreateIndexBuffer(36);

	unsigned int indices[] = {
		0, 1, 2, 2, 1, 3,
		4, 5, 6, 6, 5, 7,
		8, 9, 10, 10, 11, 8,
		12, 13, 14, 13, 15, 14,
		16, 17, 18, 17, 19, 18,
		20, 21, 22, 21, 20, 23
	};

	hr = _indexBuffer->Resource->Map(0, nullptr, &data);
	if (SUCCEEDED(hr)) {
		memcpy(data, indices, sizeof(indices));
		_indexBuffer->Resource->Unmap(0, nullptr);
	}
}

void Cube::Render()const {
	int blendMode = 0;
	if (Renderer::IsGeometryPass() && blendMode != 0) return;
	if (!Renderer::IsGeometryPass() && blendMode == 0) return;

	auto cmdList = Renderer::GetCommandListDX12();
	if (!cmdList) return;

	// _obt@r[ݒ
	D3D12_VERTEX_BUFFER_VIEW vbView = {};
	vbView.BufferLocation = _vertexBuffer->Resource->GetGPUVirtualAddress();
	vbView.StrideInBytes = _vertexBuffer->Stride;
	vbView.SizeInBytes = _vertexBuffer->Stride * _vertexBuffer->Size;
	cmdList->IASetVertexBuffers(0, 1, &vbView);

	// CfbNXobt@r[ݒ
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


	// svZ
	XMMATRIX translation = XMMatrixTranslation(_owner->_transform._position.x, _owner->_transform._position.y, _owner->_transform._position.z);
	XMFLOAT4 q = XMFLOAT4(_owner->_transform._quaternion.x, _owner->_transform._quaternion.y, _owner->_transform._quaternion.z, _owner->_transform._quaternion.w);
	XMMATRIX rotation = XMMatrixRotationQuaternion(XMLoadFloat4(&q));
	XMMATRIX scaling = XMMatrixScaling(_owner->_transform._scale.x, _owner->_transform._scale.y, _owner->_transform._scale.z);
	XMMATRIX worldMatrix =  scaling * rotation * translation;

	Renderer::SetWorldMatrix(worldMatrix);

	MATERIAL material = {};
	material.Diffuse = { 1.0f, 1.0f, 1.0f, 1.0f };
	material.TextureEnable = false;

	auto shaderComp = _owner->GetComponent<Shader>();
	if (shaderComp) {
		material.BaseColor = shaderComp->GetBaseColor();
		material.EmissionColor = shaderComp->GetEmissionColor();
		material.Metallic = shaderComp->GetMetallic();
		material.SpecularPbr = shaderComp->GetSpecular();
		material.Roughness = shaderComp->GetRoughness();
		material.NormalWeight = shaderComp->GetNormalWeight();
		material.ShadingModelID = shaderComp->GetShadingModelID();
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
	cmdList->DrawIndexedInstanced(36, 1, 0, 0, 0);
}

void Cube::ShowUI() {
}
