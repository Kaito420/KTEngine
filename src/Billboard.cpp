#include "Billboard.h"
#include "GameObject.h"
#include "Manager.h"
#include "Scene.h"
#include "Camera.h"

static XMMATRIX g_billboardMatrix = XMMatrixIdentity();

void Billboard::Awake()
{
	Square::Awake();

	//テクスチャ上書き
	// _texture = 
	//
}

void Billboard::Update()
{
	//カメラマトリクスからいろいろ計算
	Camera* camera = Manager::GetCurrentScene()->FindGameObjectByName<Camera>("Camera");

	XMMATRIX view = camera->GetViewMatrix();
	XMMATRIX viewInv = XMMatrixTranspose(view);
	XMFLOAT4X4 matrix;
	XMStoreFloat4x4(&matrix, viewInv);
	matrix._14 = matrix._24 = matrix._34 = 0;
	g_billboardMatrix = XMLoadFloat4x4(&matrix);
}

void Billboard::Render() const
{
	//頂点バッファ設定
	UINT stride = sizeof(Vertex);
	UINT offset = 0;
	Renderer::IASetVertexBuffers(0, 1, _vertexBuffer.GetAddressOf(), &stride, &offset);

	//マトリクス設定
	XMMATRIX translation = XMMatrixTranslation(_owner->_transform._position.x, _owner->_transform._position.y, _owner->_transform._position.z);

	KTVECTOR3 radians = { XMConvertToRadians(_owner->_transform._rotation.x),
						  XMConvertToRadians(_owner->_transform._rotation.y),
						  XMConvertToRadians(_owner->_transform._rotation.z) };

	XMMATRIX rotation = XMMatrixRotationRollPitchYaw(radians.x, radians.y, radians.z);

	XMMATRIX scale = XMMatrixScaling(_owner->_transform._scale.x, _owner->_transform._scale.y, _owner->_transform._scale.z);

	XMMATRIX worldMatrix = rotation * scale * g_billboardMatrix * translation;

	Renderer::SetWorldMatrix(worldMatrix);

	//プリミティブトポロジ設定
	Renderer::IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	MATERIAL material = {};
	material.Diffuse = { 1.0f, 1.0f, 1.0f, 1.0f };
	material.TextureEnable = true;
	Renderer::SetMaterial(material);

	Renderer::PSSetShaderResources(0, 1, &_texture);

	//ポリゴン描画
	Renderer::Draw(4, 0);

}


