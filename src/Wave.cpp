//=====================================================================================
// Wave.h
// Author:Kaito Aoki
// Date:2025/11/14
//=====================================================================================


#include "Wave.h"
#include "GameObject.h"
#include "Texture.h"
#include "ktvector.hpp"

void Wave::Awake(){
	// 頂点バッファ生成
	{
		for (int x = 0; x < 21; x++)
		{
			for (int z = 0; z < 21; z++)
			{
				m_Vertex[x][z].position =
					XMFLOAT3((x - 10) * 5.0f, 0.0f, (z - 10) * -5.0f);
				m_Vertex[x][z].normal = XMFLOAT3(0.0f, 1.0f, 0.0f);
				m_Vertex[x][z].color = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
				m_Vertex[x][z].uv = XMFLOAT2((float)x / 21, (float)z / 21);
			}
		}


		//法線ベクトル算出



		D3D11_BUFFER_DESC bd;
		ZeroMemory(&bd, sizeof(bd));
		bd.Usage = D3D11_USAGE_DYNAMIC;
		bd.ByteWidth = sizeof(Vertex) * 21 * 21;
		bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

		D3D11_SUBRESOURCE_DATA sd;
		ZeroMemory(&sd, sizeof(sd));
		sd.pSysMem = m_Vertex;

		RendererDX11::GetDevice()->CreateBuffer(&bd, &sd, &m_VertexBuffer);
	}




	// インデックスバッファ生成
	{
		unsigned int index[((21 + 1) * 2) * 20 - 2];

		int i = 0;
		for (int x = 0; x < 20; x++)
		{
			for (int z = 0; z < 21; z++)
			{
				index[i] = x * 21 + z;
				i++;

				index[i] = (x + 1) * 21 + z;
				i++;
			}

			if (x == 19)
				break;

			index[i] = (x + 1) * 21 + 20;
			i++;

			index[i] = (x + 1) * 21;
			i++;
		}

		D3D11_BUFFER_DESC bd;
		ZeroMemory(&bd, sizeof(bd));
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.ByteWidth = sizeof(unsigned int) * ((22 * 2) * 20 - 2);
		bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
		bd.CPUAccessFlags = 0;

		D3D11_SUBRESOURCE_DATA sd;
		ZeroMemory(&sd, sizeof(sd));
		sd.pSysMem = index;

		RendererDX11::GetDevice()->CreateBuffer(&bd, &sd, &m_IndexBuffer);
	}




	// テクスチャ読み込み
	m_Texture = Texture::Load("asset\\texture\\Title.jpg");
	//m_TextureEnv = Texture::Load("asset\\texture\\Sky.png");


	//Renderer::CreateVertexShader(&m_VertexShader, &m_VertexLayout,
	//	"shader\\vertexLightingVS.cso");

	//RendererDX11::CreateVertexShader(&m_VertexShader, &m_VertexLayout,
	//	"shader\\waveVS.cso");


	//Renderer::CreatePixelShader(&m_PixelShader,
	//	"shader\\vertexLightingPS.cso");

	//RendererDX11::CreatePixelShader(&m_PixelShader,
	//	"shader\\wavePS.cso");
}

void Wave::OnDestroy(){
	m_VertexBuffer->Release();
	m_Texture->Release();
	//m_EnvTexture->Release();

	if(m_VertexLayout) m_VertexLayout->Release();
	if(m_VertexShader)m_VertexShader->Release();
	if(m_PixelShader) m_PixelShader->Release();


}

void Wave::Update()
{
	for (int x = 0; x < 21; x++)
	{
		for (int z = 0; z < 21; z++)
		{

			float dx = m_Vertex[x][z].position.x - m_Vertex[0][0].position.x;
			float dz = m_Vertex[x][z].position.z - m_Vertex[0][0].position.z;
			float length = sqrtf(dx * dx + dz * dz);
			m_Vertex[x][z].position.y = sinf(m_Time * -1.0f + length * 0.1f) * 0.5f;

		}
	}

	for (int x = 1; x < 20; x++) {
		for (int z = 1; z < 20; z++) {
			KTVECTOR3 vx, vz, vn;
			vx.x = m_Vertex[x + 1][z].position.x - m_Vertex[x - 1][z].position.x;
			vx.y = m_Vertex[x + 1][z].position.y - m_Vertex[x - 1][z].position.y;
			vx.z = m_Vertex[x + 1][z].position.z - m_Vertex[x - 1][z].position.z;

			vz.x = m_Vertex[x][z - 1].position.x - m_Vertex[x][z + 1].position.x;
			vz.y = m_Vertex[x][z - 1].position.y - m_Vertex[x][z + 1].position.y;
			vz.z = m_Vertex[x][z - 1].position.z - m_Vertex[x][z + 1].position.z;

			vn = Cross(vz, vx);
			vn = vn.Normalize();
			m_Vertex[x][z].normal.x = vn.x;
			m_Vertex[x][z].normal.y = vn.y;
			m_Vertex[x][z].normal.z = vn.z;
		}
	}



	m_Time += 1.0f / 60.0f;



	//頂点データ書き換え
	D3D11_MAPPED_SUBRESOURCE msr;
	RendererDX11::GetContext()->Map(m_VertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &msr);

	Vertex* vertex = (Vertex*)msr.pData;

	memcpy(vertex, m_Vertex, sizeof(Vertex) * 21 * 21);

	RendererDX11::GetContext()->Unmap(m_VertexBuffer, 0);
}

void Wave::Render() const{
	// 入力レイアウト設定
	//RendererDX11::GetContext()->IASetInputLayout(m_VertexLayout);

	// シェーダ設定
	//RendererDX11::GetContext()->VSSetShader(m_VertexShader, NULL, 0);
	//RendererDX11::GetContext()->PSSetShader(m_PixelShader, NULL, 0);

	// ワールドマトリクス設定
	XMMATRIX world, scale, rot, trans;
	scale = XMMatrixScaling(_owner->_transform._scale.x, _owner->_transform._scale.y, _owner->_transform._scale.z);
	
	//回転行列
	XMFLOAT4 q = XMFLOAT4(_owner->_transform._quaternion.x, _owner->_transform._quaternion.y, _owner->_transform._quaternion.z, _owner->_transform._quaternion.w);

	rot = XMMatrixRotationQuaternion(XMLoadFloat4(&q));

	trans = XMMatrixTranslation(_owner->_transform._position.x, _owner->_transform._position.y, _owner->_transform._position.z);
	world = scale * rot * trans;
	RendererDX11::SetWorldMatrix(world);

	// 頂点バッファ設定
	UINT stride = sizeof(Vertex);
	UINT offset = 0;
	RendererDX11::GetContext()->IASetVertexBuffers(0, 1, &m_VertexBuffer, &stride, &offset);

	// インデックスバッファ設定
	RendererDX11::GetContext()->IASetIndexBuffer(
		m_IndexBuffer, DXGI_FORMAT_R32_UINT, 0);

	// マテリアル設定
	MATERIAL material{};
	//ZeroMemory(&material, sizeof(material));
	material.Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	material.TextureEnable = true;
	RendererDX11::SetMaterial(material);

	// テクスチャ設定
	RendererDX11::GetContext()->PSSetShaderResources(0, 1, &m_Texture);
	RendererDX11::GetContext()->PSSetShaderResources(1, 1, &m_TextureEnv);

	// プリミティブトポロジ設定
	RendererDX11::GetContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	// ポリゴン描画
	//RendererDX11::GetContext()->Draw(21 * 21, 0);
	RendererDX11::GetContext()->DrawIndexed((22 * 2) * 20 - 2, 0, 0);

}
