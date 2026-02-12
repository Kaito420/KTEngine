//=====================================================================================
// Camera.h
// Author:Kaito Aoki
// Date:2025/07/18
//=====================================================================================

#ifndef _CAMERA_H
#define _CAMERA_H

#include "GameObject.h"
#include "RendererDX11.h"
#include <cereal/types/base_class.hpp>
#include <cereal/types/polymorphic.hpp>

class Camera : public GameObject {
	friend class cereal::access;
protected:
	XMMATRIX _viewMatrix;
	XMMATRIX _projectionMatrix;

	float _distance = 100.0f;

public:
	void Awake() override;
	//void Start() override;
	void Update() override;
	//void LateUpdate() override;
	//void Render()const  override;

	const XMMATRIX GetViewMatrix() { return _viewMatrix; }
	const XMMATRIX GetProjectionMatrix() { return _projectionMatrix; }

	template <class Archive>
	void serialize(Archive& ar) {
		ar(cereal::base_class<GameObject>(this));
		ar(cereal::make_nvp("Distance", _distance));
	}
};

#endif // !_CAMERA_H