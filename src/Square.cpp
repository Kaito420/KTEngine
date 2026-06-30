#include "Square.h"
#include "GameObject.h"
#include "Input.h"
#include "Texture.h"
#include "ShaderManager.h"
#include "Shader.h"

void Square::Awake()
{
	_vertexBuffer = Renderer::CreateVertexBuffer(sizeof(Vertex), 4);

	Vertex v[4] = {
		{ {-0.5f, +0.5f, 0.0f}, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 0.0f} },
		{ {+0.5f, +0.5f, 0.0f}, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {1.0f, 0.0f} },
		{ {-0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 1.0f} },
		{ {+0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {1.0f, 1.0f} },
	};

	void* data = nullptr;
	HRESULT hr = _vertexBuffer->Resource->Map(0, nullptr, &data);
	if (SUCCEEDED(hr)) {
		memcpy(data, v, sizeof(v));
		_vertexBuffer->Resource->Unmap(0, nullptr);
	}

	_texture = Texture::Load("asset/texture/Brick.jpg");
}

void Square::Update(){
}

void Square::Render()const{
	auto cmdList = Renderer::GetCommandListDX12();
	if (!cmdList) return;

	// _obt@r[ݒ
	D3D12_VERTEX_BUFFER_VIEW vbView = {};
	vbView.BufferLocation = _vertexBuffer->Resource->GetGPUVirtualAddress();
	vbView.StrideInBytes = _vertexBuffer->Stride;
	vbView.SizeInBytes = _vertexBuffer->Stride * _vertexBuffer->Size;
	cmdList->IASetVertexBuffers(0, 1, &vbView);

	// g|W
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
		ID3D12PipelineState* pso = ShaderManager::Instance().GetPipelineState(
			vsId, psId, 1, 1, true, true, D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE
		);
		cmdList->SetPipelineState(pso);
	}


	// svZ
	XMMATRIX translation = XMMatrixTranslation(_owner->_transform._position.x, _owner->_transform._position.y, _owner->_transform._position.z);
	XMFLOAT4 q = XMFLOAT4(_owner->_transform._quaternion.x, _owner->_transform._quaternion.y, _owner->_transform._quaternion.z, _owner->_transform._quaternion.w);
	XMMATRIX rotation = XMMatrixRotationQuaternion(XMLoadFloat4(&q));
	XMMATRIX scale = XMMatrixScaling(_owner->_transform._scale.x, _owner->_transform._scale.y, _owner->_transform._scale.z);
	XMMATRIX worldMatrix = scale * rotation * translation;

	// 萔obt@oCh
	Renderer::SetConstant(0, &worldMatrix, sizeof(worldMatrix));

	MATERIAL material = {};
	material.Diffuse = { 1.0f, 1.0f, 1.0f, 1.0f };
	material.TextureEnable = (_texture != nullptr);
	Renderer::SetConstant(3, &material, sizeof(material));
	
	// eNX`oCh
	if (_texture) {
		Renderer::SetTexture(6, _texture);
	}

	// `
	cmdList->DrawInstanced(4, 1, 0, 0);
}
