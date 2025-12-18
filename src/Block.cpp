#include "Block.h"
#include "Cube.h"
#include "Sphere.h"
#include "Collider.h"
#include "RigidBody.h"
#include "Piece.h"

void Block::Awake()
{
	AddComponent<Sphere>();
	AddComponent<ColliderSphere>();
	AddComponent<RigidBody>();
	_piece = AddComponent<Piece>();

	_transform._scale = _transform._scale * (float)_piece->_type * 0.5f;
}

void Block::Update()
{
	if(_transform._position.y < -10.0f){
		Destroy();
	}
}
