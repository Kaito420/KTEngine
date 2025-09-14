#include "Fade.h"
#include "GameObject.h"
#include "Manager.h"
#include "Scene.h"
#include "SceneTitle.h"
#include "SceneGame.h"
#include "SceneResult.h"

void Fade::Update()
{
	_alpha += _fadeSpeed;
	{
		if (_alpha > 1.0f)
		{
			_alpha = 1.0f;
			_owner->Destroy();
		}
	}
}

void Fade::Render() const
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

	MATERIAL material = {};
	material.Diffuse = { _color, _color, _color, _alpha };
	material.TextureEnable = false;
	RendererDX11::SetMaterial(material);

	RendererDX11::GetContext()->PSSetShaderResources(0, 1, &_texture);

	//ポリゴン描画
	RendererDX11::GetContext()->Draw(4, 0);
}

void Fade::OnDestroy()
{
	auto scene = Manager::GetCurrentScene();

	if (auto title = std::dynamic_pointer_cast<SceneTitle>(scene))
		Manager::ChangeScene<SceneGame>();
	else if (auto result = std::dynamic_pointer_cast<SceneResult>(scene))
		Manager::ChangeScene<SceneTitle>();
}
