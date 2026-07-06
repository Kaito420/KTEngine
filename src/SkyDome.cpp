#include "SkyDome.h"
#include "GameObject.h"
#include "Manager.h"
#include "Scene.h"
#include "Camera.h"
#include "Texture.h"
#include "Shader.h"

void SkyDome::Awake(){
	_executeInEditor = true;	// エディターで実行
	
	if (!_sphere) {
		_sphere = new Sphere();
	}

	if (_owner) {
		_sphere->SetOwner(_owner);
		_owner->_transform._scale = { 500.0f, 500.0f, 500.0f };

		auto shaderComp = _owner->GetComponent<Shader>();
		if (!shaderComp) {
			shaderComp = _owner->AddComponent<Shader>();
		}
		if (shaderComp && shaderComp->GetTexturePath().empty()) {
			shaderComp->SetTexturePath("asset/texture/Space.jpg");
		}
	}

	_sphere->Awake();
}

void SkyDome::Update()
{
	if (Manager::GetCurrentScene() && Manager::GetCurrentScene()->FindGameObjectByName<Camera>("Camera")) {
		_owner->_transform._position = Manager::GetCurrentScene()->FindGameObjectByName<Camera>("Camera")->_transform._position;
	}
}

void SkyDome::Render() const
{
	Renderer::SetDepthEnable(false);
	Renderer::SetCullModeFront();
	_sphere->Render();
	Renderer::SetCullModeBack();
	Renderer::SetDepthEnable(true);
}
