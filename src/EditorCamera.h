#ifndef _EDITOR_CAMERA_H
#define _EDITOR_CAMERA_H

#include <DirectXMath.h>

class EditorCamera {
private:
	DirectX::XMMATRIX _viewMatrix;
	DirectX::XMMATRIX _projectionMatrix;

	DirectX::XMFLOAT3 _position = { 0.0f, 2.0f, -10.0f };
	DirectX::XMFLOAT3 _rotation = { 0.0f, 0.0f, 0.0f };
	float _distance = 100.0f;

public:
	EditorCamera();
	~EditorCamera();

	void Update();

	const DirectX::XMMATRIX& GetViewMatrix() const { return _viewMatrix; }
	const DirectX::XMMATRIX& GetProjectionMatrix() const { return _projectionMatrix; }
};

#endif // !_EDITOR_CAMERA_H
