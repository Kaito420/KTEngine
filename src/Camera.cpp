//=====================================================================================
// Camera.cpp
// Author:Kaito Aoki
// Date:2025/07/18
//=====================================================================================

#include "Camera.h"
#include "Input.h"
#include <Windows.h>

void Camera::Awake() {
	//一旦初期化
}

void Camera::Update() {
	//ここでRendererDX11のViewとProjectionを更新する

	//ラジアンに変換
	float pitch = XMConvertToRadians(_transform._rotation.x);
	float yaw = XMConvertToRadians(_transform._rotation.y);
	float roll = XMConvertToRadians(_transform._rotation.z);

	//回転行列作成
	XMMATRIX rotMtx = XMMatrixRotationRollPitchYaw(pitch, yaw, roll);

	//ベクトルに変換
	XMVECTOR frontVec = XMVector3TransformNormal(XMVectorSet(0, 0, 1, 0), rotMtx);
	XMVECTOR rightVec = XMVector3TransformNormal(XMVectorSet(1, 0, 0, 0), rotMtx);
	XMVECTOR upVec = XMVector3TransformNormal(XMVectorSet(0, 1, 0, 0), rotMtx);

	frontVec = XMVector3Normalize(frontVec);
	rightVec = XMVector3Normalize(rightVec);
	upVec = XMVector3Normalize(upVec);

	XMFLOAT3 frontTemp, rightTemp, upTemp;

	XMStoreFloat3(&frontTemp, frontVec);
	XMStoreFloat3(&rightTemp, rightVec);
	XMStoreFloat3(&upTemp, upVec);

	KTVECTOR3 frontKTVec(frontTemp.x, frontTemp.y, frontTemp.z);
	KTVECTOR3 rightKTVec(rightTemp.x, rightTemp.y, rightTemp.z);
	KTVECTOR3 upKTVec(upTemp.x, upTemp.y, upTemp.z);

	XMVECTOR position = XMVectorSet(_transform._position.x, _transform._position.y, _transform._position.z, 1.0f);

	XMMATRIX view = XMMatrixLookAtLH(
		position, // カメラ位置
		position + frontVec * _distance, // 注視点
		upVec  // 上方向
	);
	XMMATRIX projection = XMMatrixPerspectiveFovLH(XMConvertToRadians(60.0f), 1280.0f / 720.0f, 0.1f, 1000.0f);

	RendererDX11::SetViewMatrix(view);
	RendererDX11::SetProjectionMatrix(projection);

	_viewMatrix = view;
	_projectionMatrix = projection;

	if (Input::IsMouseButtonDown(Input::MouseButton::Right)) {
		_transform._rotation.y += (float)Input::GetMouseDelta().first * 0.1f;
		_transform._rotation.x += (float)Input::GetMouseDelta().second * 0.1f;
	}

	if (Input::IsMouseButtonDown(Input::MouseButton::Middle)) {
		_transform._position += upKTVec * (float)Input::GetMouseDelta().second * 0.1f;
		_transform._position += rightKTVec * -(float)Input::GetMouseDelta().first * 0.1f;
	}

	if (Input::GetMouseWheelDelta() != 0) {
		_transform._position += frontKTVec * (float)Input::GetMouseWheelDelta() * 0.03f;
	}
}
