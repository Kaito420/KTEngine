#include "Cube.h"
#include "GameObject.h"

void Cube::Awake() {
	_vertexBuffer = Renderer::CreateVertexBuffer(sizeof(Vertex), 24);

	Vertex v[] = {
		// 前の面
		{ { -0.5f,  0.5f, -0.5f },{0.0f, 0.0f, -1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f} },
		{ {  0.5f,  0.5f, -0.5f },{0.0f, 0.0f, -1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {1.0f, 0.0f} },
		{ { -0.5f, -0.5f, -0.5f },{0.0f, 0.0f, -1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 1.0f} },
		{ {  0.5f, -0.5f, -0.5f },{0.0f, 0.0f, -1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {1.0f, 1.0f} },

		// 右の面
		{ { 0.5f,  0.5f, -0.5f },{1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f}, {0.0f, 0.0f} },
		{ { 0.5f,  0.5f,  0.5f },{1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f}, {1.0f, 0.0f} },
		{ { 0.5f, -0.5f, -0.5f },{1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f}, {0.0f, 1.0f} },
		{ { 0.5f, -0.5f,  0.5f },{1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f}, {1.0f, 1.0f} },

		// 上の面
		{ { 0.5f,  0.5f, -0.5f },{0.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 1.0f, 1.0f}, {0.0f, 0.0f} },
		{ { -0.5f, 0.5f, -0.5f },{0.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 1.0f, 1.0f}, {1.0f, 0.0f} },
		{ { -0.5f, 0.5f,  0.5f },{0.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 1.0f, 1.0f}, {0.0f, 1.0f} },
		{ { 0.5f,  0.5f,  0.5f },{0.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 1.0f, 1.0f}, {1.0f, 1.0f} },

		// 後ろの面
		{ { -0.5f,  0.5f, 0.5f },{0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f, 1.0f}, {0.0f, 0.0f} },
		{ { -0.5f, -0.5f, 0.5f },{0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f, 1.0f}, {1.0f, 0.0f} },
		{ {  0.5f,  0.5f, 0.5f },{0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f, 1.0f}, {0.0f, 1.0f} },
		{ {  0.5f, -0.5f, 0.5f },{0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f, 1.0f}, {1.0f, 1.0f} },

		// 左の面
		{ { -0.5f,  0.5f, -0.5f },{-1.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 0.0f, 1.0f}, {0.0f, 0.0f} },
		{ { -0.5f, -0.5f, -0.5f },{-1.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 0.0f, 1.0f}, {1.0f, 0.0f} },
		{ { -0.5f,  0.5f,  0.5f },{-1.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 0.0f, 1.0f}, {0.0f, 1.0f} },
		{ { -0.5f, -0.5f,  0.5f },{-1.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 0.0f, 1.0f}, {1.0f, 1.0f} },

		// 下の面
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
	auto cmdList = Renderer::GetCommandListDX12();
	if (!cmdList) return;

	// 頂点バッファビュー設定
	D3D12_VERTEX_BUFFER_VIEW vbView = {};
	vbView.BufferLocation = _vertexBuffer->Resource->GetGPUVirtualAddress();
	vbView.StrideInBytes = _vertexBuffer->Stride;
	vbView.SizeInBytes = _vertexBuffer->Stride * _vertexBuffer->Size;
	cmdList->IASetVertexBuffers(0, 1, &vbView);

	// インデックスバッファビュー設定
	D3D12_INDEX_BUFFER_VIEW ibView = {};
	ibView.BufferLocation = _indexBuffer->Resource->GetGPUVirtualAddress();
	ibView.SizeInBytes = sizeof(unsigned int) * _indexBuffer->Size;
	ibView.Format = DXGI_FORMAT_R32_UINT;
	cmdList->IASetIndexBuffer(&ibView);

	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// 行列計算
	XMMATRIX translation = XMMatrixTranslation(_owner->_transform._position.x, _owner->_transform._position.y, _owner->_transform._position.z);
	XMFLOAT4 q = XMFLOAT4(_owner->_transform._quaternion.x, _owner->_transform._quaternion.y, _owner->_transform._quaternion.z, _owner->_transform._quaternion.w);
	XMMATRIX rotation = XMMatrixRotationQuaternion(XMLoadFloat4(&q));
	XMMATRIX scaling = XMMatrixScaling(_owner->_transform._scale.x, _owner->_transform._scale.y, _owner->_transform._scale.z);
	XMMATRIX worldMatrix =  scaling * rotation * translation;

	Renderer::SetConstant(0, &worldMatrix, sizeof(worldMatrix));

	MATERIAL material = {};
	material.Diffuse = { 1.0f, 1.0f, 1.0f, 1.0f };
	material.TextureEnable = false;
	Renderer::SetConstant(1, &material, sizeof(material));

	cmdList->DrawIndexedInstanced(36, 1, 0, 0, 0);
}

void Cube::ShowUI() {
}
