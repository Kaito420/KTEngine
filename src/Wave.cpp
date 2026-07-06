#include "Wave.h"
#include "GameObject.h"
#include "Texture.h"
#include "ktvector.hpp"
#include "ShaderManager.h"
#include "Shader.h"

void Wave::Awake(){
	for (int x = 0; x < 21; x++)
	{
		for (int z = 0; z < 21; z++)
		{
			m_Vertex[x][z].position =
				XMFLOAT3((x - 10) * 5.0f, 0.0f, (z - 10) * -5.0f);
			m_Vertex[x][z].normal = XMFLOAT3(0.0f, 1.0f, 0.0f);
			m_Vertex[x][z].color = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
			m_Vertex[x][z].uv = XMFLOAT2((float)x / 21, (float)z / 21);
		}
	}

	m_vertexBuffer = Renderer::CreateVertexBuffer(sizeof(Vertex), 21 * 21);
	void* data = nullptr;
	HRESULT hr = m_vertexBuffer->Resource->Map(0, nullptr, &data);
	if (SUCCEEDED(hr)) {
		memcpy(data, m_Vertex, sizeof(m_Vertex));
		m_vertexBuffer->Resource->Unmap(0, nullptr);
	}

	// CfbNXobt@
	unsigned int index[((21 + 1) * 2) * 20 - 2];
	int i = 0;
	for (int x = 0; x < 20; x++)
	{
		for (int z = 0; z < 21; z++)
		{
			index[i] = x * 21 + z;
			i++;
			index[i] = (x + 1) * 21 + z;
			i++;
		}
		if (x == 19)
			break;
		index[i] = (x + 1) * 21 + 20;
		i++;
		index[i] = (x + 1) * 21;
		i++;
	}

	m_indexBuffer = Renderer::CreateIndexBuffer(((22 * 2) * 20 - 2));
	hr = m_indexBuffer->Resource->Map(0, nullptr, &data);
	if (SUCCEEDED(hr)) {
		memcpy(data, index, sizeof(index));
		m_indexBuffer->Resource->Unmap(0, nullptr);
	}

	m_texture = Texture::Load("asset/texture/KTEngine.png");
}

void Wave::OnDestroy(){
	m_vertexBuffer.reset();
	m_indexBuffer.reset();
}

void Wave::Update()
{
	for (int x = 0; x < 21; x++)
	{
		for (int z = 0; z < 21; z++)
		{
			float dx = m_Vertex[x][z].position.x - m_Vertex[0][0].position.x;
			float dz = m_Vertex[x][z].position.z - m_Vertex[0][0].position.z;
			float length = sqrtf(dx * dx + dz * dz);
			m_Vertex[x][z].position.y = sinf(m_Time * -1.0f + length * 0.1f) * 0.5f;
		}
	}

	for (int x = 1; x < 20; x++) {
		for (int z = 1; z < 20; z++) {
			KTVECTOR3 vx, vz, vn;
			vx.x = m_Vertex[x + 1][z].position.x - m_Vertex[x - 1][z].position.x;
			vx.y = m_Vertex[x + 1][z].position.y - m_Vertex[x - 1][z].position.y;
			vx.z = m_Vertex[x + 1][z].position.z - m_Vertex[x - 1][z].position.z;

			vz.x = m_Vertex[x][z - 1].position.x - m_Vertex[x][z + 1].position.x;
			vz.y = m_Vertex[x][z - 1].position.y - m_Vertex[x][z + 1].position.y;
			vz.z = m_Vertex[x][z - 1].position.z - m_Vertex[x][z + 1].position.z;

			vn = Cross(vz, vx);
			vn = vn.Normalize();
			m_Vertex[x][z].normal.x = vn.x;
			m_Vertex[x][z].normal.y = vn.y;
			m_Vertex[x][z].normal.z = vn.z;
		}
	}

	m_Time += 1.0f / 60.0f;

	void* data = nullptr;
	HRESULT hr = m_vertexBuffer->Resource->Map(0, nullptr, &data);
	if (SUCCEEDED(hr)) {
		memcpy(data, m_Vertex, sizeof(Vertex) * 21 * 21);
		m_vertexBuffer->Resource->Unmap(0, nullptr);
	}
}

void Wave::Render() const{
	int blendMode = 1;
	if (Renderer::IsGeometryPass() && blendMode != 0) return;
	if (!Renderer::IsGeometryPass() && blendMode == 0) return;

	auto cmdList = Renderer::GetCommandListDX12();
	if (!cmdList) return;

	D3D12_VERTEX_BUFFER_VIEW vbView = {};
	vbView.BufferLocation = m_vertexBuffer->Resource->GetGPUVirtualAddress();
	vbView.StrideInBytes = m_vertexBuffer->Stride;
	vbView.SizeInBytes = m_vertexBuffer->Stride * m_vertexBuffer->Size;
	cmdList->IASetVertexBuffers(0, 1, &vbView);

	D3D12_INDEX_BUFFER_VIEW ibView = {};
	ibView.BufferLocation = m_indexBuffer->Resource->GetGPUVirtualAddress();
	ibView.SizeInBytes = sizeof(unsigned int) * m_indexBuffer->Size;
	ibView.Format = DXGI_FORMAT_R32_UINT;
	cmdList->IASetIndexBuffer(&ibView);

	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	// PSOバインド
	{
		std::string vsId = "UnlitTextureVS";
		std::string psId = "UnlitTexturePS";
		auto shaderComp = _owner->GetComponent<Shader>();
		if (shaderComp) {
			vsId = shaderComp->GetVertexShaderID();
			psId = shaderComp->GetPixelShaderID();
		}
		ID3D12PipelineState* pso = ShaderManager::Instance().GetPipelineState(vsId, psId, 1, Renderer::GetCullModeDX12(), Renderer::GetDepthEnableDX12(), Renderer::GetDepthWriteDX12(), D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
		if (pso == nullptr) return;
		cmdList->SetPipelineState(pso);
	}


	XMMATRIX world, scale, rot, trans;
	scale = XMMatrixScaling(_owner->_transform._scale.x, _owner->_transform._scale.y, _owner->_transform._scale.z);
	XMFLOAT4 q = XMFLOAT4(_owner->_transform._quaternion.x, _owner->_transform._quaternion.y, _owner->_transform._quaternion.z, _owner->_transform._quaternion.w);
	rot = XMMatrixRotationQuaternion(XMLoadFloat4(&q));
	trans = XMMatrixTranslation(_owner->_transform._position.x, _owner->_transform._position.y, _owner->_transform._position.z);
	world = scale * rot * trans;

	Renderer::SetWorldMatrix(world);

	MATERIAL material{};
	material.Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	material.TextureEnable = (m_texture != nullptr);

	auto shaderComp = _owner->GetComponent<Shader>();
	if (shaderComp) {
		material.BaseColor = shaderComp->GetBaseColor();
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

	if (m_texture) {
		Renderer::SetTexture(6, m_texture);
	}
	if (m_textureEnv) {
		Renderer::SetTexture(5, m_textureEnv);
	}

	cmdList->DrawIndexedInstanced(((22 * 2) * 20 - 2), 1, 0, 0, 0);
}
