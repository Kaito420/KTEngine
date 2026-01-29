//=====================================================================================
// Light.cpp
// Author:Kaito Aoki
// Date:2026/01/29
//=====================================================================================

#include "Light.h"

void Light::Awake(){

}

void Light::Update(){
	RendererDX11::SetLight(_lightData);
}
