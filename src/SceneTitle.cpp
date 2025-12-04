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

	GameObject* sphere1 = AddGameObject<GameObject>();
	sphere1->_name = "Sphere1";
	sphere1->_transform._position = { 0.0f, 2.0f, 0.0f };
	sphere1->AddComponent<Sphere>();
	sphere1->AddComponent<RigidBody>();
	sphere1->AddComponent<ColliderSphere>();

	GameObject* sphere2 = AddGameObject<GameObject>();
	sphere2->_name = "Sphere2";
	sphere2->_transform._position = { 0.0f, 0.0f, 0.0f };
	sphere2->AddComponent<Sphere>();
	sphere2->AddComponent<RigidBody>();
	sphere2->AddComponent<ColliderSphere>();


	//AddGameObject<GameObject>()->_name = "Cube1";
	//FindGameObjectByName<GameObject>("Cube1")->AddComponent<Cube>();
	//FindGameObjectByName<GameObject>("Cube1")->AddComponent<ColliderBox>();
	//RigidBody* rb1 = FindGameObjectByName<GameObject>("Cube1")->AddComponent<RigidBody>();
	//rb1->_useGravity = false;
	//rb1->_mass = 10000.0f;

	//AddGameObject<GameObject>()->_name = "Cube2";
	//FindGameObjectByName<GameObject>("Cube2")->_transform._position = { 0.0f,1.5f,0.0f };
	//FindGameObjectByName<GameObject>("Cube2")->AddComponent<Cube>();
	//FindGameObjectByName<GameObject>("Cube2")->AddComponent<ColliderBox>();
	//FindGameObjectByName<GameObject>("Cube2")->AddComponent<RigidBody>();

	//AddGameObject<GameObject>()->_name = "Cube3";
	//FindGameObjectByName<GameObject>("Cube3")->_transform._position = { 0.0f,2.5f,0.0f };
	//FindGameObjectByName<GameObject>("Cube3")->AddComponent<Cube>();
	//FindGameObjectByName<GameObject>("Cube3")->AddComponent<ColliderBox>();
	//FindGameObjectByName<GameObject>("Cube3")->AddComponent<RigidBody>();

	GameObject* title = AddGameObject<GameObject>();
	title->_name = "Title";
	title->_transform._position = { -1.0f, 0.5f, 3.0f };
	title->_transform._scale = { 4.5f, 2.5f, 1.0f };
	title->AddComponent<Square>()->_texture = Texture::Load("asset/texture/Title.jpg");
	title->AddComponent<TitleManager>();

}
