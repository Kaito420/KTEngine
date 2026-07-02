#include "BillboardEffect.h"
#include "Texture.h"
#include "Manager.h"
#include "Scene.h"
#include "Camera.h"
#include "ShaderManager.h"
#include "Shader.h"

static XMMATRIX g_billboardMatrix = XMMatrixIdentity();

void BillboardEffect::Awake()
{
	_vertexBuffer = Renderer::CreateVertexBuffer(sizeof(Vertex), 4);

	Vertex vertex[4];
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

	void* data = nullptr;
	HRESULT hr = _vertexBuffer->Resource->Map(0, nullptr, &data);
	if (SUCCEEDED(hr)) {
		memcpy(data, vertex, sizeof(vertex));
		_vertexBuffer->Resource->Unmap(0, nullptr);
	}

	_texture = Texture::Load("asset/texture/explosion.png");
	_numXCut = 4;
	_numYCut = 4;
	_loop = true;
	_maxFrame = _numXCut * _numYCut;
	_frame = 0;
}

void BillboardEffect::Update()
{
	Camera* camera = Manager::GetCurrentScene()->FindGameObjectByName<Camera>("Camera");
	if (!camera) return;

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
	float w = 1.0 / _numXCut;
	float h = 1.0 / _numYCut;

	float u = (_frame % _numXCut) * w;
	float v = (_frame / _numXCut) * h;

	Vertex vertex[4];
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

	void* data = nullptr;
	HRESULT hr = _vertexBuffer->Resource->Map(0, nullptr, &data);
	if (SUCCEEDED(hr)) {
		memcpy(data, vertex, sizeof(vertex));
		_vertexBuffer->Resource->Unmap(0, nullptr);
	}

	auto cmdList = Renderer::GetCommandListDX12();
	if (!cmdList) return;

	D3D12_VERTEX_BUFFER_VIEW vbView = {};
	vbView.BufferLocation = _vertexBuffer->Resource->GetGPUVirtualAddress();
	vbView.StrideInBytes = _vertexBuffer->Stride;
	vbView.SizeInBytes = _vertexBuffer->Stride * _vertexBuffer->Size;
	cmdList->IASetVertexBuffers(0, 1, &vbView);

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
		ID3D12PipelineState* pso = ShaderManager::Instance().GetPipelineState(vsId, psId, 1, Renderer::GetCullModeDX12(), Renderer::GetDepthEnableDX12(), Renderer::GetDepthWriteDX12(), D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
		if (pso == nullptr) return;
		cmdList->SetPipelineState(pso);
	}


	XMMATRIX translation = XMMatrixTranslation(_owner->_transform._position.x, _owner->_transform._position.y, _owner->_transform._position.z);
	KTVECTOR3 radians = { XMConvertToRadians(_owner->_transform._rotation.x),
						  XMConvertToRadians(_owner->_transform._rotation.y),
						  XMConvertToRadians(_owner->_transform._rotation.z) };
	XMMATRIX rotation = XMMatrixRotationRollPitchYaw(radians.x, radians.y, radians.z);
	XMMATRIX scale = XMMatrixScaling(_owner->_transform._scale.x, _owner->_transform._scale.y, _owner->_transform._scale.z);
	XMMATRIX worldMatrix = rotation * scale * g_billboardMatrix * translation;

	Renderer::SetWorldMatrix(worldMatrix);

	MATERIAL material = {};
	material.Diffuse = { 1.0f, 1.0f, 1.0f, 1.0f };
	material.TextureEnable = (_texture != nullptr);
	Renderer::SetConstant(3, &material, sizeof(material));

	if (_texture) {
		Renderer::SetTexture(6, _texture);
	}

		Renderer::BindShaderConstantsDX12();
cmdList->DrawInstanced(4, 1, 0, 0);
}

void BillboardEffect::SetEffect(const char* fileName, int xCut, int yCut, bool loop)
{
	_texture = Texture::Load(fileName);
	_numXCut = xCut;
	_numYCut = yCut;
	_loop = loop;
	_maxFrame = xCut * yCut;
}
