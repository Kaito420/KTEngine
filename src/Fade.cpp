#include "Fade.h"
#include "GameObject.h"
#include "Manager.h"
#include "Camera.h"
#include "Scene.h"
#include "SceneTitle.h"
#include "SceneGame.h"
#include "SceneResult.h"
#include "ShaderManager.h"
#include "Shader.h"

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
			vsId, psId, 1, 1, false, false, D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE
		);
		cmdList->SetPipelineState(pso);
	}


	Camera* camera = Manager::GetCurrentScene()->FindGameObjectByName<Camera>("Camera");
	if (!camera) return;

	XMMATRIX translation = XMMatrixTranslation(camera->_transform._position.x, camera->_transform._position.y, camera->_transform._position.z + 0.2f);
	KTVECTOR3 radians = { XMConvertToRadians(_owner->_transform._rotation.x),
						  XMConvertToRadians(_owner->_transform._rotation.y),
						  XMConvertToRadians(_owner->_transform._rotation.z) };
	XMMATRIX rotation = XMMatrixRotationRollPitchYaw(radians.x, radians.y, radians.z);
	XMMATRIX scale = XMMatrixScaling(_owner->_transform._scale.x, _owner->_transform._scale.y, _owner->_transform._scale.z);
	XMMATRIX worldMatrix = scale * rotation * translation;

	Renderer::SetConstant(0, &worldMatrix, sizeof(worldMatrix));

	MATERIAL material = {};
	material.Diffuse = { _color, _color, _color, _alpha };
	material.TextureEnable = (_texture != nullptr);
	Renderer::SetConstant(3, &material, sizeof(material));

	if (_texture) {
		Renderer::SetTexture(6, _texture);
	}

	cmdList->DrawInstanced(4, 1, 0, 0);
}

void Fade::OnDestroy()
{
	auto scene = Manager::GetCurrentScene();

	if (auto title = std::dynamic_pointer_cast<Scene>(scene))
		Manager::ChangeScene<SceneGame>();
	else if (auto result = std::dynamic_pointer_cast<SceneResult>(scene))
		Manager::ChangeScene<SceneTitle>();
}
