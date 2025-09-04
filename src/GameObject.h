//=====================================================================================
// GameObject.h
// Author:Kaito Aoki
// Date:2025/06/25
//=====================================================================================

#ifndef _GAMEOBJECT_H
#define _GAMEOBJECT_H

#include "ktvector.hpp"
#include <list>
#include <memory>
#include "Component.h"

class GameObject {
private:
	bool _active = true; //アクティブ状態（true:有効, false:無効）
	bool _awakened = false; //Awakeが実行されたかどうか
	bool _started = false; //Startが実行されたかどうか
protected:
	bool _destroy = false;
	std::list<std::unique_ptr<Component>> _components; //このGameObjectにアタッチされているコンポーネントのリスト

public:
	uint32_t _id;
	std::string _name = "gameObject";
	struct Transform {
		KTVECTOR3 _position = { 0.0f, 0.0f, 0.0f };
		KTVECTOR3 _scale = { 1.0f, 1.0f, 1.0f };
		KTVECTOR3 _rotation = { 0.0f, 0.0f, 0.0f };	//Degree
	};
	Transform _transform; //位置、スケール、回転を保持するTransform構造体
	virtual ~GameObject() {}

	bool Active(bool active) { _active = active; return _active; }
	bool GetActive() const{ return _active; }
	bool Awakened() { return _awakened = true; }
	bool GetAwakened() { return _awakened; }
	bool Started() { return _started = true; }
	bool GetStarted() { return _started; }

	/// <summary>
	/// インスタンス生成直後に実行（コンポーネント有効無効に関係なく呼ばれる）
	/// </summary>
	virtual void Awake() {}

	/// <summary>
	/// 最初のUpdate直前に実行（初回のみ、非アクティブの際は無視）
	/// </summary>
	virtual void Start() {}

	/// <summary>
	/// 毎フレーム実行（非アクティブの際は無視）
	/// </summary>
	virtual void Update() {}

	/// <summary>
	/// //Update後に実行（非アクティブの際は無視）
	/// </summary>
	virtual void LateUpdate() {} 

	/// <summary>
	/// LateUpdate後に実行（非アクティブの際は無視）
	/// </summary>
	virtual void Render() const {}

	/// <summary>
	/// 削除直前に実行（非アクティブの際は無視）
	/// </summary>
	virtual void OnDestroy() {}

	/// <summary>
	/// コンポーネントを追加する
	/// </summary>
	template <typename T, typename... Args>
	std::shared_ptr<T> AddComponent(Args&&... args) {
		static_assert(std::is_base_of<Component, T>::value,"T must be derived from Component");
		auto component = std::make_unique<T>(std::forward<Args>(args)...);
		component->_owner = std::make_unique<GameObject>(this); //コンポーネントにこのGameObjectのポインタを設定
		component->Active(true); //追加したコンポーネントを有効化
		component->Awake();
		component->Awakened();
		_components.push_back(component);
		return component;
	}

};


#endif // !_GAMEOBJECT_H