#include "SceneTitle.h"
#include "Camera.h"
#include "Square.h"
#include "Texture.h"
#include "TitleManager.h"

void SceneTitle::Initialize()
{
	_physicsSystem = new PhysicsSystem();

	AddGameObject<Camera>()->_name = "Camera";

	GameObject* title = AddGameObject<GameObject>();
	title->_name = "Title";
	title->_transform._position = { -1.0f, 0.5f, 3.0f };
	title->_transform._scale = { 4.5f, 2.5f, 1.0f };
	title->AddComponent<Square>()->_texture = Texture::Load("asset/texture/Title.jpg");
	title->AddComponent<TitleManager>();

}
