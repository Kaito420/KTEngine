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
#include <DirectXMath.h>
#include <string>
#include <cereal/types/list.hpp>
#include <cereal/types/memory.hpp>
#include <cereal/types/string.hpp>
#include <cereal/archives/json.hpp>

using namespace DirectX;
class Collider;

class GameObject {
	friend class cereal::access;
private:

	bool _awakened = false; //Awakeが実行されたかどうか
	bool _started = false; //Startが実行されたかどうか
protected:
	bool _destroy = false;

public:
	uint32_t _id;
	std::string _name = "gameObject";
	bool _active = true; //アクティブ状態（true:有効, false:無効）
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
	Transform _transform; //位置、スケール、回転を保持するTransform構造体
	virtual ~GameObject() {}

	bool Active(bool active) { _active = active; return _active; }
	bool GetActive() const { return _active; }
	bool Awakened() { return _awakened = true; }
	bool GetAwakened() { return _awakened; }
	bool Started() { return _started = true; }
	bool GetStarted() { return _started; }

	void Destroy() { _destroy = true; }
	bool IsDestroy() const { return _destroy; }


	KTVECTOR3 GetRight() const {
		XMMATRIX matrix;
		matrix = XMMatrixRotationRollPitchYaw(XMConvertToRadians(_transform._rotation.x), XMConvertToRadians(_transform._rotation.y), XMConvertToRadians(_transform._rotation.z));
		KTVECTOR3 right;
		XMStoreFloat3((XMFLOAT3*)&right, matrix.r[0]);
		return right.Normalize();
	}

	KTVECTOR3 GetLeft() const {
		return -GetRight().Normalize();
	}

	KTVECTOR3 GetUp() const {
		XMMATRIX matrix;
		matrix = XMMatrixRotationRollPitchYaw(XMConvertToRadians(_transform._rotation.x), XMConvertToRadians(_transform._rotation.y), XMConvertToRadians(_transform._rotation.z));
		KTVECTOR3 up;
		XMStoreFloat3((XMFLOAT3*)&up, matrix.r[1]);
		return up.Normalize();
	}

	KTVECTOR3 GetDown() const {
		return -GetUp().Normalize();
	}

	KTVECTOR3 GetForward() const {
		XMMATRIX matrix;
		matrix = XMMatrixRotationRollPitchYaw(XMConvertToRadians(_transform._rotation.x), XMConvertToRadians(_transform._rotation.y), XMConvertToRadians(_transform._rotation.z));
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

	std::shared_ptr<GameObject> Clone()const {
		auto newObj = std::make_shared<GameObject>(*this);

		//コンポーネントのコピー
		newObj->_components.clear();
		for (auto& comp : _components) {
			auto newComp = comp->Clone();
			newComp->_owner = newObj.get();	//_ownerはクローンしたGameObjectになるので設定
			newObj->_components.push_back(newComp);
		}
		return newObj;
	}

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
	/// コンポーネントのLateUpdateを呼び出す
	/// 
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

	void ProcessDestroyComponents() {
		for (auto& component : _components) {
			component->OnDestroy();
		}
	}

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
		ar(cereal::make_nvp("Transform", _transform));
		ar(cereal::make_nvp("Components", _components));

		if (std::is_same<Archive, cereal::JSONInputArchive>::value ||
			std::is_same<Archive, cereal::BinaryInputArchive>::value) {
			for(auto& component: _components){
				component->SetOwner(this); //ロード時に_ownerを再設定
			}
		}
	}

};


#endif // !_GAMEOBJECT_H