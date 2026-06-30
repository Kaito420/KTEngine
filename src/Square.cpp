#include "Square.h"
#include "GameObject.h"
#include "Input.h"
#include "Texture.h"

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

	// 頂点バッファビュー設定
	D3D12_VERTEX_BUFFER_VIEW vbView = {};
	vbView.BufferLocation = _vertexBuffer->Resource->GetGPUVirtualAddress();
	vbView.StrideInBytes = _vertexBuffer->Stride;
	vbView.SizeInBytes = _vertexBuffer->Stride * _vertexBuffer->Size;
	cmdList->IASetVertexBuffers(0, 1, &vbView);

	// トポロジ
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	// 行列計算
	XMMATRIX translation = XMMatrixTranslation(_owner->_transform._position.x, _owner->_transform._position.y, _owner->_transform._position.z);
	XMFLOAT4 q = XMFLOAT4(_owner->_transform._quaternion.x, _owner->_transform._quaternion.y, _owner->_transform._quaternion.z, _owner->_transform._quaternion.w);
	XMMATRIX rotation = XMMatrixRotationQuaternion(XMLoadFloat4(&q));
	XMMATRIX scale = XMMatrixScaling(_owner->_transform._scale.x, _owner->_transform._scale.y, _owner->_transform._scale.z);
	XMMATRIX worldMatrix = scale * rotation * translation;

	// 定数バッファバインド
	Renderer::SetConstant(0, &worldMatrix, sizeof(worldMatrix));

	MATERIAL material = {};
	material.Diffuse = { 1.0f, 1.0f, 1.0f, 1.0f };
	material.TextureEnable = (_texture != nullptr);
	Renderer::SetConstant(1, &material, sizeof(material));
	
	// テクスチャバインド
	if (_texture) {
		Renderer::SetTexture(4, _texture);
	}

	// 描画
	cmdList->DrawInstanced(4, 1, 0, 0);
}
