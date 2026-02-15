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
#include "SkyDome.h"

void Sky::Awake(){
	_executeInEditor = true;	//エディタモードでも実行する

	AddComponent<SkyDome>();
}

void Sky::Update(){

}

void Sky::Render() const{

}
