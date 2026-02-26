//=====================================================================================
// Capsule.cpp
// Author:Kaito Aoki
// Date:2026/02/26
//=====================================================================================

#include "Capsule.h"
#include "GameObject.h"
#include "ktvector.hpp"
#include "Texture.h"

void Capsule::CreateCapsuleMesh(float height, float radius, int latiudes, int longitudes, std::vector<Vertex>& vertices, std::vector<UINT>& indices){
	float cylinderHeight = (std::max)(0.0f, height - 2.0f * radius);
	float yOffset = cylinderHeight * 0.5f;

	int rings = latiudes + 1;
	//Creating Vertex Buffer
	for (int i = 0; i <= rings; ++i) {
		float v = (float)i / rings;	//uv座標
		float phi = 0.0f;
		float currentYOffset = 0.0f;

		if (i <= latiudes / 2) {//上半球
			phi = ((float)i / latiudes) * XM_PI;
			currentYOffset = yOffset;
		}
		else {//下半球
			phi = ((float)(i - 1) / latiudes) * XM_PI;
			currentYOffset = -yOffset;
		}
		for (int j = 0; j <= longitudes; ++j) {
			float u = (float)j / longitudes;//uv座標
			float theta = u * XM_2PI;

			//座標計算
			float x = radius * sin(phi) * cos(theta);
			float y = radius * cos(phi);
			float z = radius * sin(phi) * sin(theta);

			//頂点設定
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

	//Creating Index Buffer
	for (int i = 0; i < rings; ++i) {
		for (int j = 0; j < longitudes; ++j) {
			int first = (i * (longitudes + 1)) + j;
			int second = first + longitudes + 1;

			// 三角形1
			indices.push_back(first);
			indices.push_back(second);
			indices.push_back(first + 1);

			// 三角形2
			indices.push_back(second);
			indices.push_back(second + 1);
			indices.push_back(first + 1);
		}
	}
}

void Capsule::Awake(){
	std::vector<Vertex> vertices;
	std::vector<UINT> indices;
	CreateCapsuleMesh(_height, _radius, _latiudes, _longitudes, vertices, indices);
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
	_texture = Texture::Load("asset\\texture\\default.png");

}

void Capsule::Render() const{
	UINT stride = sizeof(Vertex);
	UINT offset = 0;

	RendererDX11::GetContext()->IASetVertexBuffers(0, 1, _vertexBuffer.GetAddressOf(), &stride, &offset);
	RendererDX11::GetContext()->IASetIndexBuffer(_indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);


	//マトリクス設定
	//平行移動行列
	XMMATRIX translation = XMMatrixTranslation(_owner->_transform._position.x, _owner->_transform._position.y, _owner->_transform._position.z);

	//回転行列
	XMFLOAT4 q = XMFLOAT4(_owner->_transform._quaternion.x, _owner->_transform._quaternion.y, _owner->_transform._quaternion.z, _owner->_transform._quaternion.w);

	XMMATRIX rotation = XMMatrixRotationQuaternion(XMLoadFloat4(&q));

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
	RendererDX11::GetContext()->PSSetShaderResources(0, 1, _texture.GetAddressOf());

	// ポリゴン描画
	RendererDX11::GetContext()->DrawIndexed(_indexCount, 0, 0);

}

void Capsule::ShowUI(){

}
