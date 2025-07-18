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
private:
	XMMATRIX _viewMatrix;
	XMMATRIX _projectionMatrix;

public:
	void Awake() override;
	void Start() override;
	void Update() override;
	void LateUpdate() override;
	void Render() override;

};

#endif // !_CAMERA_H