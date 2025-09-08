//=====================================================================================
// Component.h
// Author:Kaito Aoki
// Date:2025/08/14
//=====================================================================================

#ifndef _COMPONENT_H_
#define _COMPONENT_H_

#include <memory>
#include <string>
class GameObject;

class Component {
	friend class GameObject;
protected:

	bool _awakened = false;
	bool _started = false;
	GameObject* _owner = nullptr;
public:
	bool _active = false;//アクティブ状態（true:有効, false:無効）

	virtual ~Component() {}

	bool Active(bool active) { _active = active; return _active; }
	bool GetActive() { return _active; }
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
	virtual void Render()const {}

	/// <summary>
	/// 削除直前に実行（非アクティブの際は無視）
	/// </summary>
	virtual void OnDestroy() {}

	/// <summary>
	/// コンポーネントの情報をインスペクター上に表示
	/// </summary>
	virtual void ShowUI() {}

	virtual std::string GetComponentName() { return "default"; }
};

#endif // !_COMPONENT_H_