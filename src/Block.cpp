#include "Block.h"
#include "Cube.h"
#include "Collider.h"
#include "RigidBody.h"
#include "Piece.h"

void Block::Awake()
{
	AddComponent<Cube>();
	AddComponent<ColliderBox>();
	AddComponent<RigidBody>();
	AddComponent<Piece>();
}
