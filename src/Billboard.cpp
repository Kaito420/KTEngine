#include "Billboard.h"
#include "GameObject.h"
#include "Manager.h"
#include "Scene.h"
#include "Camera.h"
#include "ShaderManager.h"
#include "Shader.h"

static XMMATRIX g_billboardMatrix = XMMatrixIdentity();

void Billboard::Awake()
{
	Square::Awake();
}

void Billboard::Update()
{
	Camera* camera = Manager::GetCurrentScene()->FindGameObjectByName<Camera>("Camera");
	if (!camera) return;

	XMMATRIX view = camera->GetViewMatrix();
	XMMATRIX viewInv = XMMatrixTranspose(view);
	XMFLOAT4X4 matrix;
	XMStoreFloat4x4(&matrix, viewInv);
	matrix._14 = matrix._24 = matrix._34 = 0;
	g_billboardMatrix = XMLoadFloat4x4(&matrix);
}

void Billboard::Render() const
{
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
		ID3D12PipelineState* pso = ShaderManager::Instance().GetPipelineState(
			vsId, psId, 1, 1, true, false, D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE
		);
		cmdList->SetPipelineState(pso);
	}


	XMMATRIX translation = XMMatrixTranslation(_owner->_transform._position.x, _owner->_transform._position.y, _owner->_transform._position.z);
	KTVECTOR3 radians = { XMConvertToRadians(_owner->_transform._rotation.x),
						  XMConvertToRadians(_owner->_transform._rotation.y),
						  XMConvertToRadians(_owner->_transform._rotation.z) };
	XMMATRIX rotation = XMMatrixRotationRollPitchYaw(radians.x, radians.y, radians.z);
	XMMATRIX scale = XMMatrixScaling(_owner->_transform._scale.x, _owner->_transform._scale.y, _owner->_transform._scale.z);
	XMMATRIX worldMatrix = rotation * scale * g_billboardMatrix * translation;

	Renderer::SetConstant(0, &worldMatrix, sizeof(worldMatrix));

	MATERIAL material = {};
	material.Diffuse = { 1.0f, 1.0f, 1.0f, 1.0f };
	material.TextureEnable = (_texture != nullptr);
	Renderer::SetConstant(3, &material, sizeof(material));

	if (_texture) {
		Renderer::SetTexture(6, _texture);
	}

	cmdList->DrawInstanced(4, 1, 0, 0);
}
