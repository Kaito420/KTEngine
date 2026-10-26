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
	const DirectX::XMFLOAT3& GetPosition() const { return _position; }
	void SetPosition(const DirectX::XMFLOAT3& pos) { _position = pos; }
	const DirectX::XMFLOAT3& GetRotation() const { return _rotation; }
	void SetRotation(const DirectX::XMFLOAT3& rot) { _rotation = rot; }
	float GetDistance() const { return _distance; }
	void SetDistance(float dist) { _distance = dist; }
};

#endif // !_EDITOR_CAMERA_H
