#include "Particle.h"
#include "Camera.h"
#include "Manager.h"
#include "Scene.h"
#include "Texture.h"

static XMMATRIX g_billboardMtx = XMMatrixIdentity();

void Particle::Awake()
{
	ID3D11Device* pDevice = RendererDX11::GetDevice();
	// 頂点バッファ生成
	D3D11_BUFFER_DESC bd;
	ZeroMemory(&bd, sizeof(bd));
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(Vertex) * 4;	//四角形用
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = 0;



	Vertex vertex[4];

	vertex[0].position = XMFLOAT3(-0.1f,  0.1f, 0.0f);
	vertex[1].position = XMFLOAT3( 0.1f,  0.1f, 0.0f);
	vertex[2].position = XMFLOAT3(-0.1f, -0.1f, 0.0f);
	vertex[3].position = XMFLOAT3( 0.1f, -0.1f, 0.0f);

	vertex[0].uv = XMFLOAT2(0.0f, 0.0f);
	vertex[1].uv = XMFLOAT2(1.0f, 0.0f);
	vertex[2].uv = XMFLOAT2(0.0f, 1.0f);
	vertex[3].uv = XMFLOAT2(1.0f, 1.0f);

	for (int i = 0; i < 4; i++) {
		vertex[i].color = XMFLOAT4(1, 1, 1, 1);
		vertex[i].normal = XMFLOAT3(0.0f, 1.0f, 0.0f);
	}

	D3D11_SUBRESOURCE_DATA sd;
	sd.pSysMem = vertex;

	RendererDX11::GetDevice()->CreateBuffer(&bd, &sd, &_VertexBuffer);

	_texture = Texture::Load("asset/texture/particle.png");



	for (int i = 0; i < PARTICLE_MAX; i++) {
		_particle[i].enable = false;
	}
}

void Particle::Update()
{
	//カメラマトリクスからいろいろ計算
	Camera* camera = Manager::GetCurrentScene()->FindGameObjectByName<Camera>("Camera");

	XMMATRIX view = camera->GetViewMatrix();
	XMMATRIX viewInv = XMMatrixTranspose(view);
	XMFLOAT4X4 matrix;
	XMStoreFloat4x4(&matrix, viewInv);
	matrix._14 = matrix._24 = matrix._34 = 0;
	g_billboardMtx = XMLoadFloat4x4(&matrix);
	for (int i = 0; i < PARTICLE_MAX; i++) {
		if (_particle[i].enable == false) {
			_particle[i].enable = true;
			_particle[i].Life = 20;
			_particle[i].Position = _owner->_transform._position;//生まれた瞬間親の位置
			_particle[i].Velocity = KTVECTOR3((rand() % 100 - 50) / 5000.0f
				, (rand() % 100 + 50) / 500.0f
				, (rand() % 100 - 50) / 5000.0f);

			break;
		}
	}

	for (int i = 0; i < PARTICLE_MAX; i++) {
		if (_particle[i].enable == true) {
			_particle[i].Velocity += KTVECTOR3(0.0f, -0.01f, 0.0f);
			_particle[i].Position += _particle[i].Velocity;

			_particle[i].Life--;
			if (_particle[i].Life == 0)
				_particle[i].enable = false;
		}
	}
}

void Particle::Render() const
{
	// 頂点バッファ設定
	UINT stride = sizeof(Vertex);
	UINT offset = 0;
	RendererDX11::GetContext()->IASetVertexBuffers(0, 1, &_VertexBuffer, &stride, &offset);


	//テクスチャ設定
	RendererDX11::GetContext()->PSSetShaderResources(0, 1, &_texture);



	// プリミティブトポロジ設定
	RendererDX11::GetContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	// マテリアル設定
	MATERIAL material;
	ZeroMemory(&material, sizeof(material));
	material.Diffuse = XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f);
	material.TextureEnable = true;
	RendererDX11::SetMaterial(material);

	RendererDX11::SetDepthEnable(false);
	RendererDX11::SetCullModeFront();

	for (int i = 0; i < PARTICLE_MAX; i++) {
		if (_particle[i].enable == true) {
			//マトリクス設定
			XMMATRIX translation = XMMatrixTranslation( _particle[i].Position.x,
				 _particle[i].Position.y,
				 _particle[i].Position.z);

			KTVECTOR3 radians = { XMConvertToRadians(_owner->_transform._rotation.x),
								  XMConvertToRadians(_owner->_transform._rotation.y),
								  XMConvertToRadians(_owner->_transform._rotation.z) };

			XMMATRIX rotation = XMMatrixRotationRollPitchYaw(radians.x, radians.y, radians.z);

			XMMATRIX scale = XMMatrixScaling(1, 1, 1);

			XMMATRIX worldMatrix = rotation * scale * g_billboardMtx * translation;

			RendererDX11::SetWorldMatrix(worldMatrix);

			// ポリゴン描画
			RendererDX11::GetContext()->Draw(4, 0);
		}
	}

	RendererDX11::SetCullModeBack();

	RendererDX11::SetDepthEnable(true);
}
