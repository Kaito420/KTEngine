#include "SkyDome.h"
#include "GameObject.h"
#include "Manager.h"
#include "Scene.h"
#include "Camera.h"

void SkyDome::Update()
{
	_owner->_transform._position = Manager::GetCurrentScene()->FindGameObjectByName<Camera>("Camera")->_transform._position;
}
