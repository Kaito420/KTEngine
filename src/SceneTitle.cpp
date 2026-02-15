#include "SceneTitle.h"
#include "Camera.h"
#include "Square.h"
#include "Sphere.h"
#include "Cube.h"
#include "Texture.h"
#include "TitleManager.h"
#include "Wave.h"
#include "SkyDome.h"
#include "KillY.h"
#include "Shader.h"

void SceneTitle::Initialize()
{
	//テスト用
	//============================================
	if (!_physicsSystem)
		_physicsSystem = new PhysicsSystem();

	Camera* camera = AddGameObject<Camera>();
	camera->_name = "Camera";
	camera->_transform._position = { 0.0f, 0.0f, -21.0f };

	GameObject* skyDome = AddGameObject<GameObject>();
	skyDome->_name = "SkyDome";
	skyDome->AddComponent<SkyDome>();


	GameObject* sphereGY = AddGameObject<GameObject>();
	sphereGY->_name = "SphereGY";
	sphereGY->_transform._position = { 2.0f, 4.0f, 0.0f };
	sphereGY->AddComponent<Sphere>();
	RigidBody* sphereRb = sphereGY->AddComponent<RigidBody>();
	sphereRb->_restitution = 1.0f;
	sphereGY->AddComponent<ColliderSphere>();
	KillY* killY = sphereGY->AddComponent<KillY>();
	killY->SetThreshold(-10.0f);
	killY->SetResetPosition({ 2.0f,4.0f,0.0f });

	GameObject* cubeGY1 = AddGameObject<GameObject>();
	cubeGY1->_name = "CubeGY1";
	cubeGY1->_transform._position = { 2.0f, 2.0f, 0.0f };
	cubeGY1->_transform._scale = { 10.0f,1.0f,1.0f };
	cubeGY1->_transform._quaternion = KTQUATERNION::FromEulerAngles(0.0f, 0.0f, -5.0f);
	cubeGY1->AddComponent<Cube>();
	cubeGY1->AddComponent<ColliderBox>();
	RigidBody* rbGY1 = cubeGY1->AddComponent<RigidBody>();
	rbGY1->_useGravity = false;
	rbGY1->_mass = 10000.0f;

	GameObject* cubeGY2 = AddGameObject<GameObject>();
	cubeGY2->_name = "CubeGY2";
	cubeGY2->_transform._position = { 4.0f, -1.0f,0.0f };
	cubeGY2->_transform._scale = { 10.0f,1.0f,1.0f };
	cubeGY2->_transform._quaternion = KTQUATERNION::FromEulerAngles(0.0f, 0.0f, 5.0f);
	cubeGY2->AddComponent<Cube>();
	cubeGY2->AddComponent<ColliderBox>();
	RigidBody* rbGY2 = cubeGY2->AddComponent<RigidBody>();
	rbGY2->_useGravity = false;
	rbGY2->_mass = 10000.0f;

	GameObject* cubeGY3 = AddGameObject<GameObject>();
	cubeGY3->_name = "CubeGY3";
	cubeGY3->_transform._position = { 2.0f,-4.0f,0.0f };
	cubeGY3->_transform._scale = { 10.0f,1.0f,1.0f };
	cubeGY3->_transform._quaternion = KTQUATERNION::FromEulerAngles( 0.0f,0.0f,-5.0f);
	cubeGY3->AddComponent<Cube>();
	cubeGY3->AddComponent<ColliderBox>();
	RigidBody* rbGY3 = cubeGY3->AddComponent<RigidBody>();
	rbGY3->_useGravity = false;
	rbGY3->_mass = 10000.0f;
	
	GameObject* wave = AddGameObject<GameObject>();
	wave->_name = "Wave";
	wave->_transform._quaternion = KTQUATERNION::FromEulerAngles(-90.0f, 0.0f, 0.0f);
	wave->_transform._position = { -20.0f, 5.0f, 20.0f };
	wave->_transform._scale = { 0.32f, 1.0f, 0.18f };
	wave->AddComponent<Wave>();
	//wave->AddComponent<TitleManager>();

}
