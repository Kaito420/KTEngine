//=====================================================================================
// Scene.h
// Author:Kaito Aoki
// Date:2025/07/04
//=====================================================================================

#ifndef  _SCENE_H
#define _SCENE_H

#include <string>
#include <memory>
#include <list>
#include <unordered_map>
#include "PhysicsSystem.h"
#include "GameObject.h"
#include "PostProcessSystem.h"
#include <cereal/types/list.hpp>
#include <cereal/types/memory.hpp>

class Scene {
	friend class cereal::access;
private:
	std::string GenerateUniqueName(const std::string& baseName);

protected:
	int _selectedObjId = -1;
	int _dragSrcIndex = -1;

	bool _openPopup = false;		//�|�b�v�A�b�v���J�����}
	int _renameTargetId = -1;		
	char _renameBuffer[256] = "";

	std::list<std::shared_ptr<GameObject>> _gameObjects;
	PhysicsSystem* _physicsSystem;

public:
	virtual void Initialize();
	virtual void Finalize();
	virtual void Update();
	virtual void UpdateEditor();
	virtual void Render()const;
	virtual void RenderHierarchy();
	virtual void RenderInspector();
	virtual void RenderButton();

	virtual PhysicsSystem* GetPhysicsSystem() { return _physicsSystem; }
	std::shared_ptr<GameObject> GetSelectedGameObject();

	void OnLoaded();

	/// <summary>
	/// �Q�[���I�u�W�F�N�g��ǉ�����
	/// </summary>
	template <typename T, typename... Args>
	T* AddGameObject(Args&&... args) {
		static_assert(std::is_base_of<GameObject, T>::value, "T must inherit from GameObjcet");
		auto gameObject = std::make_shared<T>(std::forward<Args>(args)...);
		std::string baseName = typeid(T).name();
		gameObject->_name = GenerateUniqueName(baseName);
		gameObject->Active(true);
		gameObject->Awake();
		gameObject->Awakened();
		_gameObjects.push_back(gameObject);
		return gameObject.get();
	}
	
	/// <summary>
	/// �Q�[���I�u�W�F�N�g�̖��O����
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
		ar(cereal::make_nvp("PostProcessSettings", PostProcessSystem::GetSettings()));
	}

};

#endif // ! _SCENE_H

