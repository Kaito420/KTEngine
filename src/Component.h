//=====================================================================================
// Component.h
// Author:Kaito Aoki
// Date:2025/08/14
//=====================================================================================

#ifndef _COMPONENT_H_
#define _COMPONENT_H_

#include <memory>
#include <string>
#include <imgui.h>
#include "Clonable.h"

#include <cereal/types/polymorphic.hpp>

class GameObject;
class Collider;

class Component {
	friend class GameObject;
	friend class cereal::access;
protected:

	bool _awakened = false;
	bool _started = false;
	GameObject* _owner = nullptr;
public:
	bool _active = false;//アクティブ状態（true:有効, false:無効）

	bool _executeInEditor = false; //エディタモードでも実行するかどうか

	virtual ~Component() {}

	bool Active(bool active) { _active = active; return _active; }
	bool GetActive() { return _active; }
	bool Awakened() { return _awakened = true; }
	bool GetAwakened() { return _awakened; }
	bool Started() { return _started = true; }
	bool GetStarted() { return _started; }
	GameObject* GetOwner() const { return _owner; }
	void SetOwner(GameObject* gameObject) { _owner = gameObject; }

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
	/// Ownerが削除される直前に実行（非アクティブの際は無視）
	/// </summary>
	virtual void OnDestroy() {}

	/// <summary>
	/// 自分のクローンを生成する関数
	/// </summary>
	/// <returns>自分のコピー</returns>
	virtual std::shared_ptr<Component> Clone()const {
		auto newComp = std::make_shared<Component>(*this);//Component型になってしまっているのは問題
		return newComp;
	}

	/// <summary>
	/// コンポーネントの情報をインスペクター上に表示
	/// </summary>
	virtual void ShowUI() {}

	/// <summary>
	/// コンポーネントの名前を取得します。
	/// </summary>
	/// <returns>コンポーネントの名前を表す std::string 型の値。</returns>
	virtual std::string GetComponentName() { return "default"; }

	/// <summary>
	/// 衝突が発生したときに呼び出される仮想関数
	/// </summary>
	virtual void OnCollisionEnter(Collider* other){}

	/// <summary>
	/// 衝突している状態で呼び出される関数
	/// </summary>
	virtual void OnCollisionStay(Collider* other) {}

	/// <summary>
	/// 衝突が終わったときに呼び出される関数
	/// </summary>
	virtual void OnCollisionExit(Collider* other) {}

	template <class Archive>
	void serialize(Archive& ar) {
		//_ownerは保存せずにロード時に親が再設定する
		ar(cereal::make_nvp("Active", _active));
	}

};

#endif // !_COMPONENT_H_