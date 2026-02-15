//=====================================================================================
// Sky.h
// Author:Kaito Aoki
// Date:2026/02/15
//=====================================================================================

#include "Sky.h"
#include "Manager.h"
#include "Scene.h"
#include "Camera.h"
#include "Texture.h"

void Sky::Awake(){
	_executeInEditor = true;	//エディタモードでも実行する
	_sphere = new Sphere();
	_sphere->Awake();
	_sphere->SetOwner(this);
	_sphere->_texture = Texture::Load("asset\\texture\\Space.jpg");
	_transform._scale = { 500.0f,500.0f,500.0f };
}

void Sky::Update(){
	_transform._position = Manager::GetCurrentScene()->FindGameObjectByName<Camera>("Camera")->_transform._position;

}

void Sky::Render() const{
	RendererDX11::SetDepthEnable(false);
	RendererDX11::SetCullModeFront();
	_sphere->Render();
	RendererDX11::SetCullModeBack();
	RendererDX11::SetDepthEnable(true);
}
