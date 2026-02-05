//=====================================================================================
// ScenePhysicsTest.cpp
// Author:Kaito Aoki
// Date:2026/02/02
//=====================================================================================

#include "ScenePhysicsTest.h"

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

void ScenePhysicsTest::Initialize()
{
	//テスト用
	//============================================

	_physicsSystem = new PhysicsSystem();

	Camera* camera = AddGameObject<Camera>();
	camera->_name = "Camera";
	camera->_transform._position = { -9.330f, 6.362f, -24.0f };
	camera->_transform._rotation = { 7.70f, 7.40f, 0.0f };

	GameObject* skyDome = AddGameObject<GameObject>();
	skyDome->_name = "SkyDome";
	skyDome->AddComponent<SkyDome>();


	GameObject* floor = AddGameObject<GameObject>();
	floor->_name = "Floor";
	floor->_transform._position = { 0.0f, -1.5f,0.0f };
	floor->_transform._scale = { 30.0f, 1.0f, 30.0f };
	floor->AddComponent<Cube>();
	floor->AddComponent<ColliderBox>();
	RigidBody* rb = floor->AddComponent<RigidBody>();
	rb->_mass = 10000.0f;
	rb->_useGravity = false;

	int num = 0;
	for(int x = 0; x < 5; x++)
	{
		for(int y = 0; y < 5; y++)
		{
			for(int z = 0; z < 5; z++)
			{
				GameObject* gameObject = AddGameObject<GameObject>();
				std::string numS = std::to_string(num);
				gameObject->_name = "gameObject" + numS;
				gameObject->_transform._position = { (float)(x - 3) * 1.0f, (float)y * 2.0f, (float)(z - 3) * 1.0f };
				if(y % 2 != 0)
				{
					gameObject->AddComponent<Sphere>();
					auto col = gameObject->AddComponent<ColliderSphere>();
				}
				else
				{
					gameObject->AddComponent<Cube>();
					auto col = gameObject->AddComponent<ColliderBox>();
				}

				auto rb = gameObject->AddComponent<RigidBody>();
				rb->_mass = 1.0f;
				num++;
			}
		}
	}

	GameObject* wave = AddGameObject<GameObject>();
	wave->_name = "Wave";
	wave->_transform._quaternion = KTQUATERNION::FromEulerAngles(-90.0f, 0.0f, 0.0f);
	wave->_transform._position = { -20.0f, 5.0f, 20.0f };
	wave->_transform._scale = { 0.32f, 1.0f, 0.18f };
	wave->AddComponent<Wave>();

}
