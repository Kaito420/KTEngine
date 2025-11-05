#include "BlockSpawner.h"
#include "Manager.h"
#include "Scene.h"
#include "Block.h"
#include "Input.h"

void BlockSpawner::Awake()
{
}

void BlockSpawner::Start()
{
	GameObject* floor = Manager::GetCurrentScene()->FindGameObjectByName<GameObject>("Floor");
	_owner->_transform._position.y = floor->_transform._position.y + 4.0f;
}

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
		block->_transform._position = _owner->_transform._position + KTVECTOR3(0,-2.0f,0);
		//block->_transform._rotation = KTVECTOR3(0, 0, 45);
		block->_transform._quaternion = KTQUATERNION::FromEulerAngles(block->_transform._rotation.y, block->_transform._rotation.z, block->_transform._rotation.x);
		block->GetComponent<RigidBody>()->_useGravity = true;
		block->GetComponent<RigidBody>()->_restitution = 0.5f;
		_owner->_transform._position.y += 0.5f;
	}
}
