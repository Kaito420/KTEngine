//=====================================================================================
// GameObject.cpp
// Author:Kaito Aoki
// Date:2026/02/12
//=====================================================================================

#include "GameObject.h"
#include "Manager.h"

uint32_t GameObject::_nextId = 0;

void GameObject::SetWorldMatrix(const XMMATRIX& worldMat) {
	XMMATRIX localMat = worldMat;
	if (_parent) {
		XMMATRIX invParent = XMMatrixInverse(nullptr, _parent->GetWorldMatrix());
		localMat = worldMat * invParent;
	}

	XMVECTOR scaleVec, rotQuatVec, transVec;
	if (XMMatrixDecompose(&scaleVec, &rotQuatVec, &transVec, localMat)) {
		XMFLOAT3 pos, sca;
		XMFLOAT4 quat;
		XMStoreFloat3(&pos, transVec);
		XMStoreFloat3(&sca, scaleVec);
		XMStoreFloat4(&quat, rotQuatVec);

		_transform._position = KTVECTOR3(pos.x, pos.y, pos.z);
		_transform._scale = KTVECTOR3(sca.x, sca.y, sca.z);
		_transform._quaternion = KTQUATERNION(quat.x, quat.y, quat.z, quat.w).Normalize();
		_transform._rotation = _transform._quaternion.ToEulerAngles();
	}
}

void GameObject::SetParent(std::shared_ptr<GameObject> newParent, bool worldPositionStays) {
	if (_parent == newParent) return;
	if (newParent.get() == this) return;
	// 自身の子孫を親にしようとした場合は循環参照になるため拒否
	if (newParent && this->IsAncestorOf(newParent)) return;

	XMMATRIX oldWorldMat = GetWorldMatrix();

	// 旧親の_childrenリストから自身を除外
	if (_parent) {
		auto& siblings = _parent->_children;
		siblings.erase(
			std::remove_if(siblings.begin(), siblings.end(),
				[this](const std::shared_ptr<GameObject>& c) { return c.get() == this; }),
			siblings.end()
		);
	}

	_parent = newParent;
	_parentId = newParent ? (int)newParent->_id : -1;

	// 新親の_childrenリストへ自身の shared_ptr を追加
	if (_parent) {
		auto selfPtr = shared_from_this();
		auto it = std::find_if(_parent->_children.begin(), _parent->_children.end(),
			[this](const std::shared_ptr<GameObject>& c) { return c.get() == this; });
		if (it == _parent->_children.end()) {
			_parent->_children.push_back(selfPtr);
		}
	}

	if (worldPositionStays) {
		SetWorldMatrix(oldWorldMat);
	}
}

void GameObject::UpdateEditor(){

	if (_executeInEditor) {
		Update();
	}
	for (auto& component : _components) {
		if (component->GetActive() && component->_executeInEditor) {
			component->Update();
		}
	}
}

void GameObject::RemoveComponent(Component* component){
	_components.remove_if([component](const std::shared_ptr<Component>& c) {
		if (c.get() == component) {
			if (Manager::GetMode() == EngineMode::Editor) {
				c->OnDestroyOnEditor();
			}
			else {
				c->OnDestroy();
			}
			return true;
		}
		return false;
		});
}

void GameObject::ProcessDestroyComponents(){
	if (_parent) {
		auto& siblings = _parent->_children;
		siblings.erase(
			std::remove_if(siblings.begin(), siblings.end(),
				[this](const std::shared_ptr<GameObject>& c) { return c.get() == this; }),
			siblings.end()
		);
		_parent.reset();
		_parentId = -1;
	}
	for (auto& child : _children) {
		if (child) {
			child->_parent.reset();
			child->_parentId = -1;
		}
	}
	_children.clear();

	for (auto& component : _components) {
		if (Manager::GetMode() == EngineMode::Editor)
			component->OnDestroyOnEditor();
		else //EngineMode::Runtime
			component->OnDestroy();
	}
}