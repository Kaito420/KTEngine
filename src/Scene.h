//=====================================================================================
// Scene.h
// Author:Kaito Aoki
// Date:2025/07/04
//=====================================================================================

#ifndef  _SCENE_H
#define _SCENE_H

#include <memory>
#include <list>
#include <unordered_map>
#include "GameObject.h"

class Scene {
private:
	int _selectedObjId = -1;
	int _dragSrcIndex = -1;
protected:
	std::list<std::shared_ptr<GameObject>> _gameObjects;

public:
	virtual void Initialize();
	virtual void Finalize();
	virtual void Update();
	virtual void Render();
	virtual void RenderHierarchy();
	virtual void RenderInspector();

	virtual std::shared_ptr<Scene> Clone() { return nullptr; }

	template <typename T, typename... Args>
	std::shared_ptr<T> AddGameObject(Args&&... args) {
		static_assert(std::is_base_of<GameObject, T>::value, "T must inherit from GameObjcet");
		auto gameObject = std::make_shared<T>(std::forward<Args>(args)...);
		gameObject->_id = _gameObjects.size();
		gameObject->Awake();
		_gameObjects.push_back(gameObject);
		return gameObject;
	}

};

#endif // ! _SCENE_H

