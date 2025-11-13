//=====================================================================================
// Cube.cpp
// Author:Kaito Aoki
// Date:2025/09/08
//=====================================================================================

#include "Cube.h"
#include "GameObject.h"

void Cube::Awake() {

	//頂点バッファ生成
	D3D11_BUFFER_DESC bd = {};
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(Vertex) * 36; // 6 faces * 2 triangles * 3 vertices
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = 0;

	Vertex v[] = {
		//手前の面
		{ { -0.5f,  0.5f, -0.5f },{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f} },
		{ {  0.5f,  0.5f, -0.5f },{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f} },
		{ { -0.5f, -0.5f, -0.5f },{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f} },
		{ {  0.5f, -0.5f, -0.5f },{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f} },

		//右の面
		{ { 0.5f,  0.5f, -0.5f },{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f}, {0.0f, 0.0f} },
		{ { 0.5f,  0.5f,  0.5f },{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f}, {0.0f, 0.0f} },
		{ { 0.5f, -0.5f, -0.5f },{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f}, {0.0f, 0.0f} },
		{ { 0.5f, -0.5f,  0.5f },{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f}, {0.0f, 0.0f} },


		//上の面
		{ { 0.5f,  0.5f, -0.5f },{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 1.0f, 1.0f}, {0.0f, 0.0f} },
		{ { -0.5f, 0.5f, -0.5f },{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 1.0f, 1.0f}, {0.0f, 0.0f} },
		{ { -0.5f, 0.5f,  0.5f },{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 1.0f, 1.0f}, {0.0f, 0.0f} },
		{ { 0.5f,  0.5f,  0.5f },{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 1.0f, 1.0f}, {0.0f, 0.0f} },


		//奥の面
		{ { -0.5f,  0.5f, 0.5f },{0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f, 1.0f}, {0.0f, 0.0f} },
		{ { -0.5f, -0.5f, 0.5f },{0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f, 1.0f}, {0.0f, 0.0f} },
		{ {  0.5f,  0.5f, 0.5f },{0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f, 1.0f}, {0.0f, 0.0f} },
		{ {  0.5f, -0.5f, 0.5f },{0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f, 1.0f}, {0.0f, 0.0f} },

		//左の面
		{ { -0.5f,  0.5f, -0.5f },{0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 0.0f, 1.0f}, {0.0f, 0.0f} },
		{ { -0.5f, -0.5f, -0.5f },{0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 0.0f, 1.0f}, {0.0f, 0.0f} },
		{ { -0.5f,  0.5f,  0.5f },{0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 0.0f, 1.0f}, {0.0f, 0.0f} },
		{ { -0.5f, -0.5f,  0.5f },{0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 0.0f, 1.0f}, {0.0f, 0.0f} },

		//下の面
		{ {  0.5f, -0.5f, -0.5f },{0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 0.0f} },
		{ { -0.5f, -0.5f,  0.5f },{0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 0.0f} },
		{ { -0.5f, -0.5f, -0.5f },{0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 0.0f} },
		{ {  0.5f, -0.5f,  0.5f },{0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 0.0f} }
	};

	D3D11_SUBRESOURCE_DATA data;
	data.pSysMem = v;
	data.SysMemPitch = 0;
	data.SysMemSlicePitch = 0;

	RendererDX11::GetDevice()->CreateBuffer(&bd, &data, _vertexBuffer.GetAddressOf());

	//インデックスバッファ生成

	unsigned short indices[] = {
	0, 1, 2, 2, 1, 3,
	4, 5, 6, 6, 5, 7,
	8, 9, 10, 10, 11, 8,
	12, 13, 14, 13, 15, 14,
	16, 17, 18, 17, 19, 18,
	20, 21, 22, 21, 20, 23
	};

	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(unsigned short) * 36;	//バッファサイズ*頂点数分
	bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
	bd.CPUAccessFlags = 0;

	data.pSysMem = indices;
	data.SysMemPitch = 0;
	data.SysMemSlicePitch = 0;

	RendererDX11::GetDevice()->CreateBuffer(&bd, &data, _indexBuffer.GetAddressOf());
}

void Cube::Render()const {

	//頂点バッファの設定
	UINT stride = sizeof(Vertex);
	UINT offset = 0;
	RendererDX11::GetContext()->IASetVertexBuffers(0, 1, _vertexBuffer.GetAddressOf(), &stride, &offset);
	//インデックスバッファの設定
	RendererDX11::GetContext()->IASetIndexBuffer(_indexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);

	//マトリクス設定
	//平行移動行列
	XMMATRIX translation = XMMatrixTranslation(_owner->_transform._position.x, _owner->_transform._position.y, _owner->_transform._position.z);

	//回転行列
	XMFLOAT4 q = XMFLOAT4(_owner->_transform._quaternion.x, _owner->_transform._quaternion.y, _owner->_transform._quaternion.z, _owner->_transform._quaternion.w);

	XMMATRIX rotation = XMMatrixRotationQuaternion(XMLoadFloat4(&q));

	//スケーリング行列
	XMMATRIX scaling = XMMatrixScaling(_owner->_transform._scale.x, _owner->_transform._scale.y, _owner->_transform._scale.z);

	//ワールド行列
	XMMATRIX worldMatrix =  scaling * rotation * translation;

	RendererDX11::SetWorldMatrix(worldMatrix);

	MATERIAL material = {};
	material.Diffuse = { 1.0f, 1.0f, 1.0f, 1.0f };
	material.TextureEnable = false;
	RendererDX11::SetMaterial(material);

	//プリミティブトポロジの設定（三角形リスト）
	RendererDX11::GetContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	//描画コール
	RendererDX11::GetContext()->DrawIndexed(36, 0, 0); // 36 indices for the cube

}

void Cube::ShowUI() {

}