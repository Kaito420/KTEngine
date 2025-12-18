#include "SkyDome.h"
#include "GameObject.h"
#include "Manager.h"
#include "Scene.h"
#include "Camera.h"
#include "Texture.h"

void SkyDome::Awake()
{
	_sphere = new Sphere();
	_sphere->Awake();
	_sphere->SetOwner(GetOwner());
	_sphere->_texture = Texture::Load("asset\\texture\\Sky.png");
	_sphere->_radius = 1500.0f;
}

void SkyDome::Update()
{
	_owner->_transform._position = Manager::GetCurrentScene()->FindGameObjectByName<Camera>("Camera")->_transform._position;
}

void SkyDome::Render() const
{
	RendererDX11::SetDepthEnable(false);
	RendererDX11::SetCullModeFront();
	_sphere->Render();
	RendererDX11::SetCullModeBack();
	RendererDX11::SetDepthEnable(true);
}
