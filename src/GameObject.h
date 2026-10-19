//=====================================================================================
// GameObject.h
// Author:Kaito Aoki
// Date:2025/06/25
//=====================================================================================

#ifndef _GAMEOBJECT_H
#define _GAMEOBJECT_H

#include "ktvector.hpp"
#include <list>
#include <vector>
#include <algorithm>
#include <memory>
#include "Component.h"
#include <DirectXMath.h>
#include <string>
#include <cereal/types/list.hpp>
#include <cereal/types/memory.hpp>
#include <cereal/types/string.hpp>
#include <cereal/archives/json.hpp>

using namespace DirectX;
class Collider;

class GameObject : public std::enable_shared_from_this<GameObject> {
	friend class cereal::access;
	friend class Scene;
private:

	bool _awakened = false; //Awakeが実行されたかどうか
	bool _started = false; //Startが実行されたかどうか
	static uint32_t _nextId; //次に割り当てるID

	std::shared_ptr<GameObject> _parent = nullptr;
	std::vector<std::shared_ptr<GameObject>> _children;

protected:
	bool _destroy = false;

public:
	uint32_t _id;
	int _parentId = -1; // シリアライズ用親ID（-1は親なし）
	std::string _name = "gameObject";
	bool _active = true; //アクティブ状態（true:有効, false:無効）
	bool _executeInEditor = false; //エディタモードでも実行するかどうか
	std::list<std::shared_ptr<Component>> _components; //このGameObjectにアタッチされているコンポーネントのリスト

	struct Transform {
		KTVECTOR3 _position = { 0.0f, 0.0f, 0.0f };
		KTVECTOR3 _scale = { 1.0f, 1.0f, 1.0f };
		KTVECTOR3 _rotation = { 0.0f, 0.0f, 0.0f };	//Degree
		KTQUATERNION _quaternion = { 0.0f, 0.0f, 0.0f, 1.0f }; //回転を表すクォータニオン
		
		template <class Archive>
		void serialize(Archive& ar) {
			ar(cereal::make_nvp("Position", _position));
			ar(cereal::make_nvp("Scale", _scale));
			ar(cereal::make_nvp("Rotation", _rotation));
			ar(cereal::make_nvp("Quaternion", _quaternion));
		}
	};
	Transform _transform; //位置、スケール、回転を保持するTransform構造体（親がいる場合は親からの相対ローカル座標）
	GameObject() {
		_id = _nextId++;	//コンストラクタにてID決定し、次回以降のIDをインクリメント
	}
	virtual ~GameObject() {}
	virtual void ShowUI() {}

	// 自身のローカルアクティブ状態の設定・取得
	bool Active(bool active) { _active = active; return _active; }
	bool GetActiveSelf() const { return _active; }

	// 親のアクティブ状態を考慮した階層全体の有効状態（親が非アクティブなら子も非アクティブ）
	bool GetActive() const {
		if (!_active) return false;
		if (_parent) return _parent->GetActive();
		return true;
	}

	bool Awakened() { return _awakened = true; }
	bool GetAwakened() { return _awakened; }
	bool Started() { return _started = true; }
	bool GetStarted() { return _started; }

	// 削除マーク（子オブジェクトも連動して削除マークを付与）
	void Destroy() {
		_destroy = true;
		for (auto& child : _children) {
			if (child) child->Destroy();
		}
	}
	bool IsDestroy() const { return _destroy; }

	// ==== 親子関係（Hierarchy）操作メソッド ====
	std::shared_ptr<GameObject> GetParent() const { return _parent; }
	const std::vector<std::shared_ptr<GameObject>>& GetChildren() const { return _children; }

	bool IsAncestorOf(const std::shared_ptr<const GameObject>& target) const {
		std::shared_ptr<const GameObject> cur = target;
		while (cur) {
			if (cur.get() == this) return true;
			cur = cur->_parent;
		}
		return false;
	}

	void SetParent(std::shared_ptr<GameObject> newParent, bool worldPositionStays = true);

	// ==== ローカル・ワールド行列＆トランスフォーム計算 ====
	XMMATRIX GetLocalMatrix() const {
		XMMATRIX scaling = XMMatrixScaling(_transform._scale.x, _transform._scale.y, _transform._scale.z);
		XMFLOAT4 q(_transform._quaternion.x, _transform._quaternion.y, _transform._quaternion.z, _transform._quaternion.w);
		XMMATRIX rotation = XMMatrixRotationQuaternion(XMLoadFloat4(&q));
		XMMATRIX translation = XMMatrixTranslation(_transform._position.x, _transform._position.y, _transform._position.z);
		return scaling * rotation * translation;
	}

	XMMATRIX GetWorldMatrix() const {
		if (_parent) {
			return GetLocalMatrix() * _parent->GetWorldMatrix();
		}
		return GetLocalMatrix();
	}

	void SetWorldMatrix(const XMMATRIX& worldMat);

	KTVECTOR3 GetWorldPosition() const {
		if (!_parent) return _transform._position;
		XMMATRIX wm = GetWorldMatrix();
		XMFLOAT3 pos;
		XMStoreFloat3(&pos, wm.r[3]);
		return KTVECTOR3(pos.x, pos.y, pos.z);
	}

	void SetWorldPosition(const KTVECTOR3& worldPos) {
		if (!_parent) {
			_transform._position = worldPos;
		} else {
			XMMATRIX invParent = XMMatrixInverse(nullptr, _parent->GetWorldMatrix());
			XMVECTOR p = XMVectorSet(worldPos.x, worldPos.y, worldPos.z, 1.0f);
			XMVECTOR localP = XMVector3TransformCoord(p, invParent);
			XMFLOAT3 lp;
			XMStoreFloat3(&lp, localP);
			_transform._position = KTVECTOR3(lp.x, lp.y, lp.z);
		}
	}

	KTQUATERNION GetWorldQuaternion() const {
		if (!_parent) return _transform._quaternion;
		return (_parent->GetWorldQuaternion() * _transform._quaternion).Normalize();
	}

	void SetWorldQuaternion(const KTQUATERNION& worldQuat) {
		if (!_parent) {
			_transform._quaternion = worldQuat.Normalize();
		} else {
			KTQUATERNION parentWorldQuat = _parent->GetWorldQuaternion();
			_transform._quaternion = (parentWorldQuat.Inverse() * worldQuat).Normalize();
		}
		_transform._rotation = _transform._quaternion.ToEulerAngles();
	}

	KTVECTOR3 GetWorldScale() const {
		if (!_parent) return _transform._scale;
		XMMATRIX wm = GetWorldMatrix();
		float sx = XMVectorGetX(XMVector3Length(wm.r[0]));
		float sy = XMVectorGetX(XMVector3Length(wm.r[1]));
		float sz = XMVectorGetX(XMVector3Length(wm.r[2]));
		return KTVECTOR3(sx, sy, sz);
	}

	KTVECTOR3 GetRight() const {
		KTQUATERNION wq = GetWorldQuaternion();
		XMFLOAT4 q(wq.x, wq.y, wq.z, wq.w);
		XMMATRIX matrix = XMMatrixRotationQuaternion(XMLoadFloat4(&q));
		KTVECTOR3 right;
		XMStoreFloat3((XMFLOAT3*)&right, matrix.r[0]);
		return right.Normalize();
	}

	KTVECTOR3 GetLeft() const {
		return -GetRight().Normalize();
	}

	KTVECTOR3 GetUp() const {
		KTQUATERNION wq = GetWorldQuaternion();
		XMFLOAT4 q(wq.x, wq.y, wq.z, wq.w);
		XMMATRIX matrix = XMMatrixRotationQuaternion(XMLoadFloat4(&q));
		KTVECTOR3 up;
		XMStoreFloat3((XMFLOAT3*)&up, matrix.r[1]);
		return up.Normalize();
	}

	KTVECTOR3 GetDown() const {
		return -GetUp().Normalize();
	}

	KTVECTOR3 GetForward() const {
		KTQUATERNION wq = GetWorldQuaternion();
		XMFLOAT4 q(wq.x, wq.y, wq.z, wq.w);
		XMMATRIX matrix = XMMatrixRotationQuaternion(XMLoadFloat4(&q));
		KTVECTOR3 forward;
		XMStoreFloat3((XMFLOAT3*)&forward, matrix.r[2]);
		return forward.Normalize();
	}

	KTVECTOR3 GetBack() const {
		return -GetForward().Normalize();
	}

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
	/// 固定時間ステップで実行（物理更新直前、非アクティブの際は無視）
	/// </summary>
	virtual void FixedUpdate() {}

	/// <summary>
	/// エディタ上で毎フレーム実行（非アクティブの際は無視）
	/// </summary>
	virtual void UpdateEditor();

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
	T* AddComponent(Args&&... args) {
		static_assert(std::is_base_of<Component, T>::value, "T must be derived from Component");
		auto component = std::make_shared<T>(std::forward<Args>(args)...);
		component->_owner = this; //コンポーネントにこのGameObjectのポインタを設定
		component->Active(true); //追加したコンポーネントを有効化
		component->Awake();
		component->Awakened();

		_components.push_back(component);
		return component.get();
	}

	/// <summary>
	/// コンポーネントを削除する
	/// </summary>
	/// <param name="component"></param>
	void RemoveComponent(Component* component);

	/// <summary>
	/// 指定した型のコンポーネントを取得する
	/// </summary>
	template <typename T>
	T* GetComponent() {
		static_assert(std::is_base_of<Component, T>::value, "T must be derived from Component");
		for (const auto& component : _components) {
			if (auto castedComponent = dynamic_cast<T*>(component.get())) {
				return castedComponent;
			}
		}
		return nullptr; //指定した型のコンポーネントが見つからなかった場合はnullptrを返す
	}

	/// <summary>
	/// コンポーネントのUpdateを呼び出す
	/// </summary>
	void UpdateComponents() {
		for (const auto& component : _components) {
			if (component->GetActive()) {
				if (!component->GetStarted()) {
					component->Start();
					component->Started();
				}
				component->Update();
			}
		}
	}

	/// <summary>
	/// コンポーネントのFixedUpdateを呼び出す
	/// </summary>
	void FixedUpdateComponents() {
		for (const auto& component : _components) {
			if (component->GetActive()) {
				if (!component->GetStarted()) {
					component->Start();
					component->Started();
				}
				component->FixedUpdate();
			}
		}
	}

	/// <summary>
	/// コンポーネントのLateUpdateを呼び出す
	/// </summary>
	void LateUpdateComponents() {
		for (const auto& component : _components) {
			if (component->GetActive()) {
				component->LateUpdate();
			}
		}
	}

	/// <summary>
	/// コンポーネントの描画を呼び出す
	/// </summary>
	void RenderComponents() const {
		for (const auto& component : _components) {
			if (component->GetActive()) {
				component->Render();
			}
		}
	}

	void ProcessDestroyComponents();

	void DispatchOnCollisionEnter(Collider* other) {
		for (auto& component : _components) {
			if (component->GetActive()) {
				component->OnCollisionEnter(other);
			}
		}
	}

	void DispatchOnCollisionStay(Collider* other) {
		for (auto& component : _components) {
			if (component->GetActive()) {
				component->OnCollisionStay(other);
			}
		}
	}

	void DispatchOnCollisionExit(Collider* other) {
		for (auto& component : _components) {
			if (component->GetActive()) {
				component->OnCollisionExit(other);
			}
		}
	}

	template <class Archive>
	void serialize(Archive& ar) {
		ar(cereal::make_nvp("ID", _id));
		ar(cereal::make_nvp("Name", _name));
		ar(cereal::make_nvp("Active", _active));
		ar(cereal::make_nvp("ExecuteInEditor", _executeInEditor));
		ar(cereal::make_nvp("Transform", _transform));
		ar(cereal::make_nvp("Components", _components));
		try {
			ar(cereal::make_nvp("ParentID", _parentId));
		} catch (...) {
			_parentId = -1;
		}

		if (std::is_same<Archive, cereal::JSONInputArchive>::value ||
			std::is_same<Archive, cereal::BinaryInputArchive>::value) {
			for(auto& component: _components){
				component->SetOwner(this); //ロード時に_ownerを再設定
			}
		}
	}

	void OnLoaded() {
		for (auto& component : _components) {
			if (!component->GetAwakened()) {
				component->Awake();
				component->Awakened();
			}
		}
	}

};


#endif // !_GAMEOBJECT_H