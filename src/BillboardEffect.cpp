#include "BillboardEffect.h"
#include "Texture.h"
#include "Manager.h"
#include "Scene.h"
#include "Camera.h"

static XMMATRIX g_billboardMatrix = XMMatrixIdentity();

void BillboardEffect::Awake()
{
	ID3D11Device* pDevice = RendererDX11::GetDevice();
	// 頂点バッファ生成
	D3D11_BUFFER_DESC bd;
	ZeroMemory(&bd, sizeof(bd));
	bd.Usage = D3D11_USAGE_DYNAMIC;
	bd.ByteWidth = sizeof(Vertex) * 4;	//四角形用
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	RendererDX11::GetDevice()->CreateBuffer(&bd, NULL, _vertexBuffer.GetAddressOf());


	D3D11_MAPPED_SUBRESOURCE msr;
	RendererDX11::GetContext()->Map(_vertexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &msr);
	Vertex* vertex = (Vertex*)msr.pData;

	vertex[0].position = XMFLOAT3(-0.5f, +0.5f, 0.0f);
	vertex[1].position = XMFLOAT3(+0.5f, +0.5f, 0.0f);
	vertex[2].position = XMFLOAT3(-0.5f, -0.5f, 0.0f);
	vertex[3].position = XMFLOAT3(+0.5f, -0.5f, 0.0f);

	vertex[0].uv = XMFLOAT2(0.0f, 0.0f);
	vertex[1].uv = XMFLOAT2(1.0f, 0.0f);
	vertex[2].uv = XMFLOAT2(0.0f, 1.0f);
	vertex[3].uv = XMFLOAT2(1.0f, 1.0f);

	for (int i = 0; i < 4; i++) {
		vertex[i].color = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		vertex[i].normal = XMFLOAT3(0.0f, 0.0f, -1.0f);
	}
	RendererDX11::GetContext()->Unmap(_vertexBuffer.Get(), 0);

	_texture = Texture::Load("asset/texture/explosion.png");
	_numXCut = 4;
	_numYCut = 4;
	_loop = true;
	_maxFrame = _numXCut * _numYCut;
	_frame = 0;
	
}

void BillboardEffect::Update()
{
	//カメラマトリクスからいろいろ計算
	Camera* camera = Manager::GetCurrentScene()->FindGameObjectByName<Camera>("Camera");

	XMMATRIX view = camera->GetViewMatrix();
	XMMATRIX viewInv = XMMatrixTranspose(view);
	XMFLOAT4X4 matrix;
	XMStoreFloat4x4(&matrix, viewInv);
	matrix._14 = matrix._24 = matrix._34 = 0;
	g_billboardMatrix = XMLoadFloat4x4(&matrix);

	_frame++;
	if (_loop)
		_frame = _frame % _maxFrame;
	else
		if (_frame > _maxFrame)
			_owner->Destroy();
}

void BillboardEffect::Render() const
{
	RendererDX11::SetDepthEnable(false);

	//1カット当たりのwidth,hight
	float w = 1.0 / _numXCut;
	float h = 1.0 / _numYCut;

	//対応するカットの左上の座標
	float u = (_frame % _numXCut) * w;
	float v = (_frame / _numXCut) * h;

	D3D11_MAPPED_SUBRESOURCE msr;
	RendererDX11::GetContext()->Map(_vertexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &msr);
	Vertex* vertex = (Vertex*)msr.pData;

	vertex[0].position = XMFLOAT3(-0.5f, +0.5f, 0.0f);
	vertex[1].position = XMFLOAT3(+0.5f, +0.5f, 0.0f);
	vertex[2].position = XMFLOAT3(-0.5f, -0.5f, 0.0f);
	vertex[3].position = XMFLOAT3(+0.5f, -0.5f, 0.0f);

	vertex[0].uv = XMFLOAT2(u, v);
	vertex[1].uv = XMFLOAT2(u + w, v);
	vertex[2].uv = XMFLOAT2(u, v + h);
	vertex[3].uv = XMFLOAT2(u + w, v + h);

	for (int i = 0; i < 4; i++) {
		vertex[i].color = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		vertex[i].normal = XMFLOAT3(0.0f, 1.0f, 0.0f);
	}
	RendererDX11::GetContext()->Unmap(_vertexBuffer.Get(), 0);

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

	XMMATRIX worldMatrix = rotation * scale * g_billboardMatrix * translation;

	RendererDX11::SetWorldMatrix(worldMatrix);

	//プリミティブトポロジ設定
	RendererDX11::GetContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	MATERIAL material = {};
	material.Diffuse = { 1.0f, 1.0f, 1.0f, 1.0f };
	material.TextureEnable = true;
	RendererDX11::SetMaterial(material);

	RendererDX11::GetContext()->PSSetShaderResources(0, 1, &_texture);

	//ポリゴン描画
	RendererDX11::GetContext()->Draw(4, 0);

	RendererDX11::SetDepthEnable(true);

}

void BillboardEffect::SetEffect(const char* fileName, int xCut, int yCut, bool loop)
{
	_texture = Texture::Load(fileName);
	_numXCut = xCut;
	_numYCut = yCut;
	_loop = loop;
	_maxFrame = xCut * yCut;
}
