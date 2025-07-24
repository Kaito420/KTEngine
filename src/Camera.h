//=====================================================================================
// Camera.h
// Author:Kaito Aoki
// Date:2025/07/18
//=====================================================================================

#ifndef _CAMERA_H
#define _CAMERA_H

#include "GameObject.h"
#include "RendererDX11.h"


class Camera : public GameObject {
protected:
	XMMATRIX _viewMatrix;
	XMMATRIX _projectionMatrix;

	KTVECTOR3 _frontVec = { 0.0f, 0.0f, 1.0f };
	KTVECTOR3 _rightVec = { 1.0f, 0.0f, 0.0f };
	KTVECTOR3 _upVec = { 0.0f, 1.0f, 0.0f };
	float _distance = 100.0f;

public:
	void Awake() override;
	//void Start() override;
	void Update() override;
	//void LateUpdate() override;
	//void Render() override;

};

#endif // !_CAMERA_H