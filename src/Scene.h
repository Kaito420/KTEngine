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
#include "PhysicsSystem.h"
#include "GameObject.h"
#include <cereal/types/list.hpp>
#include <cereal/types/memory.hpp>

class Scene {
	friend class cereal::access;
private:


protected:
	int _selectedObjId = -1;
	int _dragSrcIndex = -1;
	std::list<std::shared_ptr<GameObject>> _gameObjects;
	PhysicsSystem* _physicsSystem;

public:
	virtual void Initialize();
	virtual void Finalize();
	virtual void Update();
	virtual void Render()const;
	virtual void RenderHierarchy();
	virtual void RenderInspector();
	virtual void RenderButton();

	virtual PhysicsSystem* GetPhysicsSystem() { return _physicsSystem; }

	void OnLoaded();

	virtual std::shared_ptr<Scene> Clone()const;

	/// <summary>
	/// ゲームオブジェクトを追加する
	/// </summary>
	template <typename T, typename... Args>
	T* AddGameObject(Args&&... args) {
		static_assert(std::is_base_of<GameObject, T>::value, "T must inherit from GameObjcet");
		auto gameObject = std::make_shared<T>(std::forward<Args>(args)...);
		gameObject->_id = _gameObjects.size();
		gameObject->_name = std::string(typeid(T).name()) + std::to_string(_gameObjects.size());
		gameObject->Active(true);
		gameObject->Awake();
		gameObject->Awakened();
		_gameObjects.push_back(gameObject);
		return gameObject.get();
	}
	
	/// <summary>
	/// ゲームオブジェクトの名前検索
	/// </summary>
	template <typename T>
	T* FindGameObjectByName(const std::string& name) {
		static_assert(std::is_base_of<GameObject, T>::value, "T must inherit from GameObject");
		for (const auto& gameObject : _gameObjects) {
			if (gameObject->_name == name) {
				return dynamic_cast<T*>(gameObject.get());
			}
		}
		return nullptr;
	}

	template <class Archive>
	void serialize(Archive& ar) {
		ar(cereal::make_nvp("GameObjects", _gameObjects));
	}

};

#endif // ! _SCENE_H

