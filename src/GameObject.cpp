//=====================================================================================
// GameObject.cpp
// Author:Kaito Aoki
// Date:2026/02/12
//=====================================================================================

#include "GameObject.h"
#include "Manager.h"

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

void GameObject::RemoveComponent(Component* component){
	_components.remove_if([component](const std::shared_ptr<Component>& c) {
		// ポインタが一致したら削除対象
		if (c.get() == component) {

			//削除前に後始末を行う(ProcessDestroyComponentsと同様のロジック)
			if (Manager::GetMode() == EngineMode::Editor) {
				c->OnDestroyOnEditor();
			}
			else {
				c->OnDestroy();
			}

			return true; //trueを返すとリストから削除される
		}
		return false;
		});
}

void GameObject::ProcessDestroyComponents(){
	for (auto& component : _components) {
		if (Manager::GetMode() == EngineMode::Editor)
			component->OnDestroyOnEditor();
		else //EngineMode::Runtime
			component->OnDestroy();
	}
}
