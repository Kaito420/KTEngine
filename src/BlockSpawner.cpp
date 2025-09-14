#include "BlockSpawner.h"
#include "Manager.h"
#include "Scene.h"
#include "Block.h"
#include "Input.h"

void BlockSpawner::Update()
{
	if (Input::IsKeyDown('W')) {
		_owner->_transform._position -= _owner->GetForward() * 0.1f;
	}
	if (Input::IsKeyDown('S')) {
		_owner->_transform._position -= _owner->GetBack() * 0.1f;
	}
	if (Input::IsKeyDown('A')) {
		_owner->_transform._position += _owner->GetLeft() * 0.1f;
	}
	if (Input::IsKeyDown('D')) {
		_owner->_transform._position += _owner->GetRight() * 0.1f;
	}
	if(Input::IsKeyPressed(VK_SPACE)){
		GameObject* block = Manager::GetCurrentScene()->AddGameObject<Block>();
		block->_transform._position = _owner->_transform._position + KTVECTOR3(0,-5.0f,0);
		block->GetComponent<RigidBody>()->_useGravity = true;
	}
}
