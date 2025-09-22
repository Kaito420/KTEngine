#include "SceneResult.h"
#include "Camera.h"
#include "Square.h"
#include "Texture.h"
#include "ResultManager.h"

void SceneResult::Initialize()
{
	_physicsSystem = new PhysicsSystem();

	AddGameObject<Camera>()->_name = "Camera";

	GameObject* result = AddGameObject<GameObject>();
	result->_name = "Result";
	result->_transform._position = { -1.0f, 0.5f, 3.0f };
	result->_transform._scale = { 4.5f, 2.5f, 1.0f };
	result->AddComponent<Square>()->_texture = Texture::Load("asset/texture/Result.jpg");
	result->AddComponent<ResultManager>();

}
