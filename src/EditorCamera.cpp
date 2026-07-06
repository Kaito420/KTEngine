#include "EditorCamera.h"
#include "Input.h"
#include "Renderer.h"

using namespace DirectX;

EditorCamera::EditorCamera() {
	_viewMatrix = XMMatrixIdentity();
	_projectionMatrix = XMMatrixIdentity();
}

EditorCamera::~EditorCamera() {}

void EditorCamera::Update() {
	// Calculate rotation matrices
	XMMATRIX rotY = XMMatrixRotationY(_rotation.y);
	XMMATRIX rotX = XMMatrixRotationX(_rotation.x);
	XMMATRIX rotZ = XMMatrixRotationZ(_rotation.z);
	XMMATRIX rotMatrix = rotZ * rotX * rotY;

	XMVECTOR frontVec = XMVector3TransformCoord(XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f), rotMatrix);
	XMVECTOR rightVec = XMVector3TransformCoord(XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f), rotMatrix);
	XMVECTOR upVec = XMVector3TransformCoord(XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), rotMatrix);

	XMFLOAT3 frontTemp, rightTemp, upTemp;
	XMStoreFloat3(&frontTemp, frontVec);
	XMStoreFloat3(&rightTemp, rightVec);
	XMStoreFloat3(&upTemp, upVec);

	// Handle Input
	if (Input::IsMouseButtonDown(Input::MouseButton::Right) && Input::IsSceneViewHovered()) {
		_rotation.y += (float)Input::GetMouseDelta().first * 0.01f;
		_rotation.x += (float)Input::GetMouseDelta().second * 0.01f;
	}

	if (Input::IsMouseButtonDown(Input::MouseButton::Middle) && Input::IsSceneViewHovered()) {
		_position.x += upTemp.x * (float)Input::GetMouseDelta().second * 0.05f;
		_position.y += upTemp.y * (float)Input::GetMouseDelta().second * 0.05f;
		_position.z += upTemp.z * (float)Input::GetMouseDelta().second * 0.05f;

		_position.x -= rightTemp.x * (float)Input::GetMouseDelta().first * 0.05f;
		_position.y -= rightTemp.y * (float)Input::GetMouseDelta().first * 0.05f;
		_position.z -= rightTemp.z * (float)Input::GetMouseDelta().first * 0.05f;
	}

	if (Input::GetMouseWheelDelta() != 0 && Input::IsSceneViewHovered()) {
		_position.x += frontTemp.x * (float)Input::GetMouseWheelDelta() * 0.003f;
		_position.y += frontTemp.y * (float)Input::GetMouseWheelDelta() * 0.003f;
		_position.z += frontTemp.z * (float)Input::GetMouseWheelDelta() * 0.003f;
	}

	XMVECTOR position = XMVectorSet(_position.x, _position.y, _position.z, 1.0f);

	_viewMatrix = XMMatrixLookAtLH(
		position,
		position + frontVec * _distance,
		upVec
	);

	float width = Renderer::GetSceneWidth();
	float height = Renderer::GetSceneHeight();
	float aspectRatio = width / height;

	if (height <= 0.0f) aspectRatio = 1.777f; //16:9

	_projectionMatrix = XMMatrixPerspectiveFovLH(
		XMConvertToRadians(60.0f),
		aspectRatio,
		0.1f,
		1000.0f);
}
