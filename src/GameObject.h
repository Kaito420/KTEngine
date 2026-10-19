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

	bool _awakened = false; //Awake縺悟ｮ溯｡後＆繧後◆縺九←縺・°
	bool _started = false; //Start縺悟ｮ溯｡後＆繧後◆縺九←縺・°
	static uint32_t _nextId; //谺｡縺ｫ蜑ｲ繧雁ｽ薙※繧紀D

	std::shared_ptr<GameObject> _parent = nullptr;
	std::vector<std::shared_ptr<GameObject>> _children;

protected:
	bool _destroy = false;

public:
	uint32_t _id;
	int _parentId = -1; // 繧ｷ繝ｪ繧｢繝ｩ繧､繧ｺ逕ｨ隕ｪID・・1縺ｯ隕ｪ縺ｪ縺暦ｼ・
	std::string _name = "gameObject";
	bool _active = true; //繧｢繧ｯ繝・ぅ繝也憾諷具ｼ・rue:譛牙柑, false:辟｡蜉ｹ・・
	bool _executeInEditor = false; //繧ｨ繝・ぅ繧ｿ繝｢繝ｼ繝峨〒繧ょｮ溯｡後☆繧九°縺ｩ縺・°
	std::list<std::shared_ptr<Component>> _components; //縺薙・GameObject縺ｫ繧｢繧ｿ繝・メ縺輔ｌ縺ｦ縺・ｋ繧ｳ繝ｳ繝昴・繝阪Φ繝医・繝ｪ繧ｹ繝・

	struct Transform {
		KTVECTOR3 _position = { 0.0f, 0.0f, 0.0f };
		KTVECTOR3 _scale = { 1.0f, 1.0f, 1.0f };
		KTVECTOR3 _rotation = { 0.0f, 0.0f, 0.0f };	//Degree
		KTQUATERNION _quaternion = { 0.0f, 0.0f, 0.0f, 1.0f }; //蝗櫁ｻ｢繧定｡ｨ縺吶け繧ｩ繝ｼ繧ｿ繝九が繝ｳ
		
		template <class Archive>
		void serialize(Archive& ar) {
			ar(cereal::make_nvp("Position", _position));
			ar(cereal::make_nvp("Scale", _scale));
			ar(cereal::make_nvp("Rotation", _rotation));
			ar(cereal::make_nvp("Quaternion", _quaternion));
		}
	};
	Transform _transform; //菴咲ｽｮ縲√せ繧ｱ繝ｼ繝ｫ縲∝屓霆｢繧剃ｿ晄戟縺吶ｋTransform讒矩菴難ｼ郁ｦｪ縺後＞繧句ｴ蜷医・隕ｪ縺九ｉ縺ｮ逶ｸ蟇ｾ繝ｭ繝ｼ繧ｫ繝ｫ蠎ｧ讓呻ｼ・
	GameObject() {
		_id = _nextId++;	//繧ｳ繝ｳ繧ｹ繝医Λ繧ｯ繧ｿ縺ｫ縺ｦID豎ｺ螳壹＠縲∵ｬ｡蝗樔ｻ･髯阪・ID繧偵う繝ｳ繧ｯ繝ｪ繝｡繝ｳ繝・
	}
	virtual ~GameObject() {}
	virtual void ShowUI() {}

	// 閾ｪ霄ｫ縺ｮ繝ｭ繝ｼ繧ｫ繝ｫ繧｢繧ｯ繝・ぅ繝也憾諷九・險ｭ螳壹・蜿門ｾ・
	bool Active(bool active) { _active = active; return _active; }
	bool GetActiveSelf() const { return _active; }

	// 隕ｪ縺ｮ繧｢繧ｯ繝・ぅ繝也憾諷九ｒ閠・・縺励◆髫主ｱ､蜈ｨ菴薙・譛牙柑迥ｶ諷具ｼ郁ｦｪ縺碁撼繧｢繧ｯ繝・ぅ繝悶↑繧牙ｭ舌ｂ髱槭い繧ｯ繝・ぅ繝厄ｼ・
	bool GetActive() const {
		if (!_active) return false;
		if (_parent) return _parent->GetActive();
		return true;
	}

	bool Awakened() { return _awakened = true; }
	bool GetAwakened() { return _awakened; }
	bool Started() { return _started = true; }
	bool GetStarted() { return _started; }

	// 蜑企勁繝槭・繧ｯ・亥ｭ舌が繝悶ず繧ｧ繧ｯ繝医ｂ騾｣蜍輔＠縺ｦ蜑企勁繝槭・繧ｯ繧剃ｻ倅ｸ趣ｼ・
	void Destroy() {
		_destroy = true;
		for (auto& child : _children) {
			if (child) child->Destroy();
		}
	}
	bool IsDestroy() const { return _destroy; }

	// ==== 隕ｪ蟄宣未菫ゑｼ・ierarchy・画桃菴懊Γ繧ｽ繝・ラ ====
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

	int GetHierarchyDepth() const {
		int depth = 0;
		auto cur = _parent;
		while (cur) {
			++depth;
			cur = cur->_parent;
		}
		return depth;
	}

	// ==== 繝ｭ繝ｼ繧ｫ繝ｫ繝ｻ繝ｯ繝ｼ繝ｫ繝芽｡悟・・・ヨ繝ｩ繝ｳ繧ｹ繝輔か繝ｼ繝險育ｮ・====
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
	/// 繧､繝ｳ繧ｹ繧ｿ繝ｳ繧ｹ逕滓・逶ｴ蠕後↓螳溯｡鯉ｼ医さ繝ｳ繝昴・繝阪Φ繝域怏蜉ｹ辟｡蜉ｹ縺ｫ髢｢菫ゅ↑縺丞他縺ｰ繧後ｋ・・
	/// </summary>
	virtual void Awake() {}

	/// <summary>
	/// 譛蛻昴・Update逶ｴ蜑阪↓螳溯｡鯉ｼ亥・蝗槭・縺ｿ縲・撼繧｢繧ｯ繝・ぅ繝悶・髫帙・辟｡隕厄ｼ・
	/// </summary>
	virtual void Start() {}

	/// <summary>
	/// 豈弱ヵ繝ｬ繝ｼ繝螳溯｡鯉ｼ磯撼繧｢繧ｯ繝・ぅ繝悶・髫帙・辟｡隕厄ｼ・
	/// </summary>
	virtual void Update() {}

	/// <summary>
	/// 蝗ｺ螳壽凾髢薙せ繝・ャ繝励〒螳溯｡鯉ｼ育黄逅・峩譁ｰ逶ｴ蜑阪・撼繧｢繧ｯ繝・ぅ繝悶・髫帙・辟｡隕厄ｼ・
	/// </summary>
	virtual void FixedUpdate() {}

	/// <summary>
	/// 繧ｨ繝・ぅ繧ｿ荳翫〒豈弱ヵ繝ｬ繝ｼ繝螳溯｡鯉ｼ磯撼繧｢繧ｯ繝・ぅ繝悶・髫帙・辟｡隕厄ｼ・
	/// </summary>
	virtual void UpdateEditor();

	/// <summary>
	/// //Update蠕後↓螳溯｡鯉ｼ磯撼繧｢繧ｯ繝・ぅ繝悶・髫帙・辟｡隕厄ｼ・
	/// </summary>
	virtual void LateUpdate() {}

	/// <summary>
	/// LateUpdate蠕後↓螳溯｡鯉ｼ磯撼繧｢繧ｯ繝・ぅ繝悶・髫帙・辟｡隕厄ｼ・
	/// </summary>
	virtual void Render() const {}

	/// <summary>
	/// 蜑企勁逶ｴ蜑阪↓螳溯｡鯉ｼ磯撼繧｢繧ｯ繝・ぅ繝悶・髫帙・辟｡隕厄ｼ・
	/// </summary>
	virtual void OnDestroy() {}



	/// <summary>
	/// 繧ｳ繝ｳ繝昴・繝阪Φ繝医ｒ霑ｽ蜉縺吶ｋ
	/// </summary>
	template <typename T, typename... Args>
	T* AddComponent(Args&&... args) {
		static_assert(std::is_base_of<Component, T>::value, "T must be derived from Component");
		auto component = std::make_shared<T>(std::forward<Args>(args)...);
		component->_owner = this; //繧ｳ繝ｳ繝昴・繝阪Φ繝医↓縺薙・GameObject縺ｮ繝昴う繝ｳ繧ｿ繧定ｨｭ螳・
		component->Active(true); //霑ｽ蜉縺励◆繧ｳ繝ｳ繝昴・繝阪Φ繝医ｒ譛牙柑蛹・
		component->Awake();
		component->Awakened();

		_components.push_back(component);
		return component.get();
	}

	/// <summary>
	/// 繧ｳ繝ｳ繝昴・繝阪Φ繝医ｒ蜑企勁縺吶ｋ
	/// </summary>
	/// <param name="component"></param>
	void RemoveComponent(Component* component);

	/// <summary>
	/// 謖・ｮ壹＠縺溷梛縺ｮ繧ｳ繝ｳ繝昴・繝阪Φ繝医ｒ蜿門ｾ励☆繧・
	/// </summary>
	template <typename T>
	T* GetComponent() {
		static_assert(std::is_base_of<Component, T>::value, "T must be derived from Component");
		for (const auto& component : _components) {
			if (auto castedComponent = dynamic_cast<T*>(component.get())) {
				return castedComponent;
			}
		}
		return nullptr; //謖・ｮ壹＠縺溷梛縺ｮ繧ｳ繝ｳ繝昴・繝阪Φ繝医′隕九▽縺九ｉ縺ｪ縺九▲縺溷ｴ蜷医・nullptr繧定ｿ斐☆
	}

	/// <summary>
	/// 繧ｳ繝ｳ繝昴・繝阪Φ繝医・Update繧貞他縺ｳ蜃ｺ縺・
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
	/// 繧ｳ繝ｳ繝昴・繝阪Φ繝医・FixedUpdate繧貞他縺ｳ蜃ｺ縺・
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
	/// 繧ｳ繝ｳ繝昴・繝阪Φ繝医・LateUpdate繧貞他縺ｳ蜃ｺ縺・
	/// </summary>
	void LateUpdateComponents() {
		for (const auto& component : _components) {
			if (component->GetActive()) {
				component->LateUpdate();
			}
		}
	}

	/// <summary>
	/// 繧ｳ繝ｳ繝昴・繝阪Φ繝医・謠冗判繧貞他縺ｳ蜃ｺ縺・
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
				component->SetOwner(this); //繝ｭ繝ｼ繝画凾縺ｫ_owner繧貞・險ｭ螳・
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