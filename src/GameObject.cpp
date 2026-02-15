//=====================================================================================
// GameObject.cpp
// Author:Kaito Aoki
// Date:2025/06/25
//=====================================================================================

#include "GameObject.h"

uint32_t GameObject::_nextId = 0;

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
