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

	GameObject* skyDome = AddGameObject<GameObject>();
	skyDome->_name = "SkyDome";
	skyDome->AddComponent<SkyDome>();
	
	GameObject* blockSpawner = AddGameObject<GameObject>();
	blockSpawner->_name = "BlockSpawner";
	blockSpawner->AddComponent<BlockSpawner>();
	blockSpawner->_transform._position = { 0.0f, 10.0f, 0.0f };
	blockSpawner->_transform._scale = { 50.0f, 50.0f, 50.0f };
	blockSpawner->_transform._rotation = { 180.0f, 0.0f, 0.0f };
	blockSpawner->AddComponent<ModelRenderer>()->Load("asset\\model\\Arrow.obj");
	blockSpawner->AddComponent<Particle>();


	GameObject* floor1 = AddGameObject<GameObject>();
	floor1->_name = "Floor1";
	floor1->_transform._position = { 0.0f, -1.5f,0.0f };
	floor1->_transform._scale = { 10.0f, 1.0f, 10.0f };
	floor1->AddComponent<Cube>();
	floor1->AddComponent<ColliderBox>();
	floor1->AddComponent<RigidBody>()->_mass = 10000;


}