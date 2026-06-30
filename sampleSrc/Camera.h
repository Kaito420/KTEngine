// ======================================================================
// Camera.h
// aurhor: Kaito Aoki
// Date: 2026/04/27
// ======================================================================

#ifndef _CAMERA_H
#define _CAMERA_H

#include "Object.h"
#include "Main.h"

class Camera : public Object {
private:
	XMMATRIX m_viewMatrix;
	XMMATRIX m_projectionMatrix;
	float m_distance = 10.0f;
public:
	Camera();
	void Update() override;
	void Draw() override;
};

#endif // !_CAMERA_H