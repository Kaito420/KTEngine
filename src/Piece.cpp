#include "Piece.h"
#include "GameObject.h"
#include "Manager.h"
#include "Scene.h"
#include "Explosion.h"


void Piece::Awake() {//ランダムに種類を決定（デバッグ用に種類を減らしている）
	_type = (Type)(rand() % (Type::Sun) + 1);
	//_type = Type::Earth;
}

void Piece::OnCollisionEnter(Collider* other)
{
	if (other->GetOwner()->GetComponent<Piece>()) {
		//GameObject* explosion = Manager::GetCurrentScene()->AddGameObject<Explosion>();
		//explosion->_transform._position = _owner->_transform._position + KTVECTOR3(0.0f, 0.0f, -1.0f);
		if (_type == other->GetOwner()->GetComponent<Piece>()->_type) {//同じ種類なら消える
			_owner->Destroy();
			other->GetOwner()->Destroy();
		}
	}
}

void Piece::ShowUI()
{
	ImGui::Text("Piece Type: %d", static_cast<int>(_type));
}


