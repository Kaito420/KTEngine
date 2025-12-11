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
	AddComponent<Piece>();
}
