//=====================================================================================
// Sphere.h
// Author:Kaito Aoki
// Date:2025/09/05
//=====================================================================================

#include "Sphere.h"
#include "GameObject.h"
#include <imgui.h>

#include "Texture.h"

void Sphere::CreateSphereMesh(float radius, int sliceCount, int stackCount, std::vector<Vertex>& vertices, std::vector<UINT>& indices){
	vertices.clear();
	indices.clear();

	//トップ
	vertices.push_back({ XMFLOAT3(0.0f, radius, 0.0f),
						 XMFLOAT3(0.0f, 1.0f, 0.0f),
						 XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f),
						 XMFLOAT2(0.0f, 0.0f)
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

	// ボトム頂点
	vertices.push_back({ XMFLOAT3(0.0f, -radius, 0.0f),
						 XMFLOAT3(0.0f, -1.0f, 0.0f),
						 XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f),
						 XMFLOAT2(0.0f, 1.0f) });

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

void Sphere::Awake() {
	std::vector<Vertex> vertices;
	std::vector<UINT> indices;
	CreateSphereMesh(_radius, _stackCount, _sliceCount, vertices, indices);
	_indexCount = indices.size();

	//頂点バッファ生成
	D3D11_BUFFER_DESC bd = {};
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(Vertex) * vertices.size();
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = 0;

	D3D11_SUBRESOURCE_DATA sd = {};
	sd.pSysMem = vertices.data();

	RendererDX11::GetDevice()->CreateBuffer(&bd, &sd, &_vertexBuffer);

	//インデックスバッファ
	bd.ByteWidth = sizeof(UINT) * indices.size();
	bd.BindFlags = D3D11_BIND_INDEX_BUFFER;

	sd.pSysMem = indices.data();

	RendererDX11::GetDevice()->CreateBuffer(&bd, &sd, &_indexBuffer);

}

void Sphere::Render()const {
	UINT stride = sizeof(Vertex);
	UINT offset = 0;

	RendererDX11::GetContext()->IASetVertexBuffers(0, 1, _vertexBuffer.GetAddressOf(), &stride, &offset);
	RendererDX11::GetContext()->IASetIndexBuffer(_indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);


	//マトリクス設定
	//平行移動行列
	XMMATRIX translation = XMMatrixTranslation(_owner->_transform._position.x, _owner->_transform._position.y, _owner->_transform._position.z);

	KTVECTOR3 radians = { XMConvertToRadians(_owner->_transform._rotation.x),
						  XMConvertToRadians(_owner->_transform._rotation.y),
						  XMConvertToRadians(_owner->_transform._rotation.z) };

	XMMATRIX rotation = XMMatrixRotationRollPitchYaw(radians.x, radians.y, radians.z);

	//スケーリング行列
	XMMATRIX scaling = XMMatrixScaling(_owner->_transform._scale.x, _owner->_transform._scale.y, _owner->_transform._scale.z);

	//ワールド行列
	XMMATRIX worldMatrix = scaling * rotation * translation;

	RendererDX11::SetWorldMatrix(worldMatrix);

	// プリミティブトポロジ設定
	RendererDX11::GetContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	MATERIAL material = {};
	material.Diffuse = { 1.0f, 1.0f, 1.0f, 1.0f };
	material.TextureEnable = true;
	RendererDX11::SetMaterial(material);

	// シェーダーリソースビュー設定
	RendererDX11::GetContext()->PSSetShaderResources(0, 1, &_texture);

	// ポリゴン描画
	RendererDX11::GetContext()->DrawIndexed(_indexCount, 0, 0);
}

void Sphere::ShowUI() {
	
}