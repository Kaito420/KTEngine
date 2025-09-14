//=====================================================================================
// SceneGame.cpp
// Author:Kaito Aoki
// Date:2025/09/14
//=====================================================================================

#include "SceneGame.h"

#include "Camera.h"
#include "Square.h"
#include "Sphere.h"
#include "Cube.h"
#include "Collider.h"
#include "RigidBody.h"
#include "ModelRenderer.h"
#include "Texture.h"
#include "SkyDome.h"
#include "Billboard.h"
#include "BlockSpawner.h"
#include "CubeKiller.h"
#include "Particle.h"

void SceneGame::Initialize() {
	_physicsSystem = new PhysicsSystem();

	Camera* camera = AddGameObject<Camera>();
	camera->_name = "Camera";
	camera->_transform._position = { -1.1f, 1.0f, -20.0f };

	
	GameObject* blockSpawner = AddGameObject<GameObject>();
	blockSpawner->_name = "BlockSpawner";
	blockSpawner->AddComponent<BlockSpawner>();
	blockSpawner->_transform._position = { 0.0f, 10.0f, 0.0f };
	blockSpawner->_transform._scale = { 50.0f, 50.0f, 50.0f };
	blockSpawner->_transform._rotation = { 180.0f, 0.0f, 0.0f };
	blockSpawner->AddComponent<ModelRenderer>()->Load("asset\\model\\Arrow.obj");
	blockSpawner->AddComponent<Particle>();


	GameObject* floor = AddGameObject<GameObject>();
	floor->_name = "Floor";
	floor->_transform._position = { 0.0f, -1.5f,0.0f };
	floor->_transform._scale = { 10.0f, 1.0f, 10.0f };
	floor->AddComponent<Cube>();
	floor->AddComponent<ColliderBox>();
	floor->AddComponent<RigidBody>()->_mass = 10000;

	GameObject* cubeKiller = AddGameObject<GameObject>();
	cubeKiller->_name = "CubeKiller";
	cubeKiller->_transform._position = { 0.0f, -5.0f, 0.0f };
	cubeKiller->_transform._scale = { 50.0f, 1.0f, 50.0f };
	cubeKiller->_transform._rotation = { 180.0f, 0.0f, 0.0f };
	cubeKiller->_transform._quaternion = KTQUATERNION::FromEulerAngles(cubeKiller->_transform._rotation.y, cubeKiller->_transform._rotation.z, cubeKiller->_transform._rotation.x);
	cubeKiller->AddComponent<CubeKiller>();
	cubeKiller->AddComponent<Cube>();
	cubeKiller->AddComponent<ColliderBox>();

	GameObject* skyDome = AddGameObject<GameObject>();
	skyDome->AddComponent<SkyDome>();
	skyDome->AddComponent<Sphere>()->_texture = Texture::Load("asset\\texture\\Sky.png");
	skyDome->_transform._scale = { 300.0f, 300.0f, 300.0f };

	GameObject* board1 = AddGameObject<GameObject>();
	board1->_name = "Board1";
	board1->_transform._position = { -5.0f, 2.0f, 0.0f };
	board1->_transform._scale = { 3.0f, 2.0f, 0.1f };
	board1->AddComponent<Billboard>()->_texture = Texture::Load("asset\\texture\\1.png");

	GameObject* board2 = AddGameObject<GameObject>();
	board2->_name = "Board2";
	board2->_transform._position = { 5.0f, 0.0f, -8.0f };
	board2->_transform._scale = { 3.0f, 2.0f, 0.1f };
	board2->AddComponent<Billboard>()->_texture = Texture::Load("asset\\texture\\2.png");

	GameObject* board3 = AddGameObject<GameObject>();
	board3->_name = "Board3";
	board3->_transform._position = { -4.0f, 0.0f, -10.0f };
	board3->_transform._scale = { 3.0f, 2.0f, 0.1f };
	board3->AddComponent<Billboard>()->_texture = Texture::Load("asset\\texture\\3.png");
	
	GameObject* board4 = AddGameObject<GameObject>();
	board4->_name = "Board4";
	board4->_transform._position = { 6.0f, 3.0f, 5.0f };
	board4->_transform._scale = { 3.0f, 2.0f, 0.1f };
	board4->AddComponent<Billboard>()->_texture = Texture::Load("asset\\texture\\4.png");

}