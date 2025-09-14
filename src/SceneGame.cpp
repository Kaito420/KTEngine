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

void SceneGame::Initialize() {
	_physicsSystem = new PhysicsSystem();

	AddGameObject<Camera>()->_name = "Camera";
	AddGameObject<GameObject>()->_name = "Square1";
	FindGameObjectByName<GameObject>("Square1")->AddComponent<Billboard>();
	AddGameObject<GameObject>()->_name = "BlockSpawner";
	FindGameObjectByName<GameObject>("BlockSpawner")->AddComponent<Sphere>()->_texture = Texture::Load("asset\\texture\\Brick.jpg");

	AddGameObject<GameObject>()->_name = "Floor";
	FindGameObjectByName<GameObject>("Floor")->_transform._position = { 0.0f, -1.5f,0.0f };
	FindGameObjectByName<GameObject>("Floor")->_transform._scale = { 10.0f, 1.0f, 10.0f };
	FindGameObjectByName<GameObject>("Floor")->AddComponent<Cube>();
	FindGameObjectByName<GameObject>("Floor")->AddComponent<ColliderBox>();
	FindGameObjectByName<GameObject>("Floor")->AddComponent<RigidBody>()->_mass = 100;



	AddGameObject<GameObject>()->_name = "SkyDome";
	FindGameObjectByName<GameObject>("SkyDome")->AddComponent<SkyDome>();
	FindGameObjectByName<GameObject>("SkyDome")->AddComponent<Sphere>()->_texture = Texture::Load("asset\\texture\\Sky.png");
	FindGameObjectByName<GameObject>("SkyDome")->_transform._scale = { 100.0f, 100.0f, 100.0f };

}