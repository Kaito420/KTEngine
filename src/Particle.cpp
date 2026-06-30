#include "Particle.h"
#include "Camera.h"
#include "Manager.h"
#include "Scene.h"
#include "Texture.h"
#include "ShaderManager.h"
#include "Shader.h"

static XMMATRIX g_billboardMtx = XMMatrixIdentity();

void Particle::Awake()
{
	_vertexBuffer = Renderer::CreateVertexBuffer(sizeof(Vertex), 4);

	Vertex vertex[4];
	vertex[0].position = XMFLOAT3(-0.1f, +0.1f, 0.0f);
	vertex[1].position = XMFLOAT3(+0.1f, +0.1f, 0.0f);
	vertex[2].position = XMFLOAT3(-0.1f, -0.1f, 0.0f);
	vertex[3].position = XMFLOAT3(+0.1f, -0.1f, 0.0f);

	vertex[0].uv = XMFLOAT2(0.0f, 0.0f);
	vertex[1].uv = XMFLOAT2(1.0f, 0.0f);
	vertex[2].uv = XMFLOAT2(0.0f, 1.0f);
	vertex[3].uv = XMFLOAT2(1.0f, 1.0f);

	for (int i = 0; i < 4; i++) {
		vertex[i].color = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		vertex[i].normal = XMFLOAT3(0.0f, 0.0f, -1.0f);
	}

	void* data = nullptr;
	HRESULT hr = _vertexBuffer->Resource->Map(0, nullptr, &data);
	if (SUCCEEDED(hr)) {
		memcpy(data, vertex, sizeof(vertex));
		_vertexBuffer->Resource->Unmap(0, nullptr);
	}

	_texture = Texture::Load("asset/texture/particle.png");

	for (int i = 0; i < PARTICLE_MAX; i++) {
		_particle[i].enable = false;
	}
}

void Particle::Update()
{
	Camera* camera = Manager::GetCurrentScene()->FindGameObjectByName<Camera>("Camera");
	if (!camera) return;

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
			_particle[i].Position = _owner->_transform._position;
			_particle[i].Velocity = KTVECTOR3((rand() % 100 - 50) / 500.0f
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
	auto cmdList = Renderer::GetCommandListDX12();
	if (!cmdList) return;

	D3D12_VERTEX_BUFFER_VIEW vbView = {};
	vbView.BufferLocation = _vertexBuffer->Resource->GetGPUVirtualAddress();
	vbView.StrideInBytes = _vertexBuffer->Stride;
	vbView.SizeInBytes = _vertexBuffer->Stride * _vertexBuffer->Size;
	cmdList->IASetVertexBuffers(0, 1, &vbView);

	if (_texture) {
		Renderer::SetTexture(6, _texture);
	}

	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	// PSOバインド
	{
		std::string vsId = "UnlitTextureVS";
		std::string psId = "UnlitTexturePS";
		auto shaderComp = _owner->GetComponent<Shader>();
		if (shaderComp) {
			vsId = shaderComp->GetVertexShaderID();
			psId = shaderComp->GetPixelShaderID();
		}
		ID3D12PipelineState* pso = ShaderManager::Instance().GetPipelineState(
			vsId, psId, 1, 1, true, false, D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE
		);
		cmdList->SetPipelineState(pso);
	}


	MATERIAL material = {};
	material.Diffuse = XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f);
	material.TextureEnable = (_texture != nullptr);
	Renderer::SetConstant(3, &material, sizeof(material));

	for (int i = 0; i < PARTICLE_MAX; i++) {
		if (_particle[i].enable == true) {
			XMMATRIX translation = XMMatrixTranslation(_particle[i].Position.x,
				 _particle[i].Position.y,
				 _particle[i].Position.z);

			KTVECTOR3 radians = { 0 ,0 ,0 };
			XMMATRIX rotation = XMMatrixRotationRollPitchYaw(radians.x, radians.y, radians.z);
			XMMATRIX scale = XMMatrixScaling(1, 1, 1);
			XMMATRIX worldMatrix = rotation * scale * g_billboardMtx * translation;

			Renderer::SetConstant(0, &worldMatrix, sizeof(worldMatrix));

			cmdList->DrawInstanced(4, 1, 0, 0);
		}
	}
}
