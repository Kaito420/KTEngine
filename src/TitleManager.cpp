#include "TitleManager.h"
#include "Manager.h"
#include "Scene.h"
#include "Input.h"
#include "FadeObject.h"
#include "ScenePhysicsTest.h"

void TitleManager::Update()
{
	GameObject* fade;
	if (Input::IsKeyPressed(VK_RETURN)) {
		fade = Manager::GetCurrentScene()->AddGameObject<FadeObject>();
	}
	if (Input::IsKeyPressed(VK_SPACE)) {
		Manager::ChangeScene<ScenePhysicsTest>();
	}
}
