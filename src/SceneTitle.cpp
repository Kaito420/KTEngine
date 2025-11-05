#include "SceneTitle.h"
#include "Camera.h"
#include "Square.h"
#include "Sphere.h"
#include "Cube.h"
#include "Texture.h"
#include "TitleManager.h"

void SceneTitle::Initialize()
{
	//テスト用
	//============================================

	_physicsSystem = new PhysicsSystem();

	AddGameObject<Camera>()->_name = "Camera";
	AddGameObject<GameObject>()->_name = "Square1";
	FindGameObjectByName<GameObject>("Square1")->AddComponent<Square>();
	AddGameObject<GameObject>()->_name = "Sphere1";
	FindGameObjectByName<GameObject>("Sphere1")->AddComponent<Sphere>();
	AddGameObject<GameObject>()->_name = "Cube1";
	FindGameObjectByName<GameObject>("Cube1")->AddComponent<Cube>();
	FindGameObjectByName<GameObject>("Cube1")->AddComponent<ColliderBox>();
	FindGameObjectByName<GameObject>("Cube1")->AddComponent<RigidBody>();


	AddGameObject<GameObject>()->_name = "Cube2";
	FindGameObjectByName<GameObject>("Cube2")->_transform._position = { 0.0f,1.5f,0.0f };
	FindGameObjectByName<GameObject>("Cube2")->AddComponent<Cube>();
	FindGameObjectByName<GameObject>("Cube2")->AddComponent<ColliderBox>();
	FindGameObjectByName<GameObject>("Cube2")->AddComponent<RigidBody>();

	GameObject* title = AddGameObject<GameObject>();
	title->_name = "Title";
	title->_transform._position = { -1.0f, 0.5f, 3.0f };
	title->_transform._scale = { 4.5f, 2.5f, 1.0f };
	title->AddComponent<Square>()->_texture = Texture::Load("asset/texture/Title.jpg");
	title->AddComponent<TitleManager>();

}
