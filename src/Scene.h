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

	bool _openPopup = false;		// ポップアップを開くかどうか
	int _renameTargetId = -1;		
	char _renameBuffer[256] = "";

	std::list<std::shared_ptr<GameObject>> _gameObjects;
	PhysicsSystem* _physicsSystem;
	float _physicsAccumulator = 0.0f;

public:
	virtual void Initialize();
	virtual void Finalize();
	virtual void Update();
	virtual void UpdateEditor();
	virtual void Render()const;
	virtual void RenderHierarchy();
	void RenderHierarchyNode(const std::shared_ptr<GameObject>& gameObject);
	virtual void RenderInspector();
	virtual void RenderButton();

	virtual PhysicsSystem* GetPhysicsSystem() { return _physicsSystem; }
	std::shared_ptr<GameObject> GetSelectedGameObject();
	int GetSelectedObjId() const { return _selectedObjId; }
	void SetSelectedObjId(int id) { _selectedObjId = id; }

	void OnLoaded();

	/// <summary>
	/// ゲームオブジェクトを追加する
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
		ar(cereal::make_nvp("PostProcessSettings", PostProcessSystem::GetSettings()));
		try {
			int subSteps = PhysicsSystem::GetSubSteps();
			int solverIters = PhysicsSystem::GetSolverIterations();
			ar(cereal::make_nvp("PhysicsSubSteps", subSteps));
			ar(cereal::make_nvp("PhysicsSolverIterations", solverIters));
			PhysicsSystem::SetSubSteps(subSteps);
			PhysicsSystem::SetSolverIterations(solverIters);
		} catch (...) {}
	}

};

#endif // ! _SCENE_H

