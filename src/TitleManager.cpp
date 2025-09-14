#include "TitleManager.h"
#include "Manager.h"
#include "Scene.h"
#include "Input.h"
#include "FadeObject.h"

void TitleManager::Update()
{
	GameObject* fade;
	if (Input::IsKeyPressed(VK_RETURN) && !_isEnter) {
		_isEnter = true;
		fade = Manager::GetCurrentScene()->AddGameObject<FadeObject>();
		fade->_transform._position = { -1.0f, 0.5f, 2.5f };
		fade->_transform._scale = { 4.5f, 2.5f, 1.0f };
	}
}
