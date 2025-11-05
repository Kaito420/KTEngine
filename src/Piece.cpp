#include "Piece.h"
#include "GameObject.h"
#include "Manager.h"
#include "Scene.h"
#include "Explosion.h"


void Piece::OnCollisionEnter(Collider* other)
{
	if (other->GetOwner()->GetComponent<Piece>()) {
		//GameObject* explosion = Manager::GetCurrentScene()->AddGameObject<Explosion>();
		//explosion->_transform._position = other->_collisionInfo._collisionPoint + KTVECTOR3(0, 0, -0.5f);
	}
}
