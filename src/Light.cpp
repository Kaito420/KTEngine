//=====================================================================================
// Light.cpp
// Author:Kaito Aoki
// Date:2026/01/29
//=====================================================================================

#include "Light.h"

void Light::Awake(){
	_lightData.Enable = true;
	_lightData.Direction = XMFLOAT4(0.0f, -1.0f, -1.0f, 0.0f);
	_lightData.Ambient = XMFLOAT4(0.2f, 0.2f, 0.2f, 1.0f);
	_lightData.Diffuse = XMFLOAT4(0.8f, 0.8f, 0.8f, 1.0f);
	_lightData.Position = XMFLOAT4(-5.0f, 10.0f, 5.0f, 0.0f);
	_lightData.Parameter = XMFLOAT4(100.0f, 1.5f, 0.0f, 0.0f);
}

void Light::Update(){
	RendererDX11::SetLight(_lightData);
}
