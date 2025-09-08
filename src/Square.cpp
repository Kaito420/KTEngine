//=====================================================================================
// Square.cpp
// Author:Kaito Aoki
// Date:2025/07/15
//=====================================================================================

#include "Square.h"
#include "GameObject.h"

void Square::Awake()
{
	D3D11_BUFFER_DESC bd = {};
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(Vertex) * 4;
	bd.BindFlags = D3D10_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = 0;

	Vertex v[4] = {
		{ {-0.5f, +0.5f, 0.0f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 0.0f} },
		{ {+0.5f, +0.5f, 0.0f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {1.0f, 0.0f} },
		{ {-0.5f, -0.5f, 0.0f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 1.0f} },
		{ {+0.5f, -0.5f, 0.0f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {1.0f, 1.0f} },
	};

	D3D11_SUBRESOURCE_DATA sd = {};
	sd.pSysMem = v;
	sd.SysMemPitch = 0;
	sd.SysMemSlicePitch = 0;

	_owner->_transform._position = { 0.0f, 0.0f, 0.0f };
	_owner->_transform._rotation = { 0.0f, 0.0f, 0.0f };

	RendererDX11::GetDevice()->CreateBuffer(&bd, &sd, _vertexBuffer.GetAddressOf());
}

void Square::Update()
{
}

void Square::Render()const
{

	//頂点バッファ設定
	UINT stride = sizeof(Vertex);
	UINT offset = 0;
	RendererDX11::GetContext()->IASetVertexBuffers(0, 1, _vertexBuffer.GetAddressOf(), &stride, &offset);

	//マトリクス設定
	XMMATRIX translation = XMMatrixTranslation(_owner->_transform._position.x, _owner->_transform._position.y, _owner->_transform._position.z);

	KTVECTOR3 radians = { XMConvertToRadians(_owner->_transform._rotation.x),
						  XMConvertToRadians(_owner->_transform._rotation.y),
						  XMConvertToRadians(_owner->_transform._rotation.z) };

	XMMATRIX rotation = XMMatrixRotationRollPitchYaw(radians.x, radians.y, radians.z);

	XMMATRIX scale = XMMatrixScaling(_owner->_transform._scale.x, _owner->_transform._scale.y, _owner->_transform._scale.z);

	XMMATRIX worldMatrix = scale * rotation * translation;

	RendererDX11::SetWorldMatrix(worldMatrix);

	//プリミティブトポロジ設定
	RendererDX11::GetContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	

	//ポリゴン描画
	RendererDX11::GetContext()->Draw(4, 0);
}
