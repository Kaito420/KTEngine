// ======================================================================
// Camera.cpp
// aurhor: Kaito Aoki
// Date: 2026/04/27
// ======================================================================

#include "Camera.h"
#include "Main.h"
#include "RenderManager.h"

Camera::Camera(){
	m_position = KTVECTOR3(0.0f, 2.0f, -3.5f);
	m_rotation = KTVECTOR3(20.0f, 0.0f, 0.0f);
	m_viewMatrix = XMMatrixIdentity();
	m_projectionMatrix = XMMatrixIdentity();
	m_distance = 10.0f;
}

void Camera::Update() {
	//ここでRendererDX11のViewとProjectionを更新する

	RenderManager* renderManager = RenderManager::GetInstance();

	//ラジアンに変換
	float pitch = XMConvertToRadians(m_rotation.x);
	float yaw = XMConvertToRadians(m_rotation.y);
	float roll = XMConvertToRadians(m_rotation.z);

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

	XMVECTOR position = XMVectorSet(m_position.x, m_position.y, m_position.z, 1.0f);

	XMMATRIX view = XMMatrixLookAtLH(
		position, // カメラ位置
		position + frontVec * m_distance, // 注視点
		upVec  // 上方向
	);

	//RendererDX11からシーンサイズを取得してアスペクト比を計算
	float width = (float)renderManager->GetBackBufferWidth();
	float height = (float)renderManager->GetBackBufferHeight();
	float aspectRatio = width / height;

	if (height <= 0.0f) aspectRatio = 1.777f; //16:9

	XMMATRIX projection = XMMatrixPerspectiveFovLH(
		XMConvertToRadians(60.0f),
		aspectRatio,
		0.1f,
		1000.0f);


	m_viewMatrix = view;
	m_projectionMatrix = projection;

	//if (Input::IsMouseButtonDown(Input::MouseButton::Right) && Input::IsGameViewHovered()) {
	//	_transform._rotation.y += (float)Input::GetMouseDelta().first * 0.1f;
	//	_transform._rotation.x += (float)Input::GetMouseDelta().second * 0.1f;
	//}

	//if (Input::IsMouseButtonDown(Input::MouseButton::Middle) && Input::IsGameViewHovered()) {
	//	_transform._position += upKTVec * (float)Input::GetMouseDelta().second * 0.1f;
	//	_transform._position += rightKTVec * -(float)Input::GetMouseDelta().first * 0.1f;
	//}

	//if (Input::GetMouseWheelDelta() != 0 && Input::IsGameViewHovered()) {
	//	_transform._position += frontKTVec * (float)Input::GetMouseWheelDelta() * 0.03f;
	//}

}

void Camera::Draw(){
	RenderManager* renderManager = RenderManager::GetInstance();
	// 定数バッファにViewとProjectionをセットしてGPUに渡す
	CAMERA_CONSTANT constant{};
	XMStoreFloat4x4(&constant.View, XMMatrixTranspose( m_viewMatrix));
	XMStoreFloat4x4(&constant.Projection, XMMatrixTranspose(m_projectionMatrix));

	constant.Position = XMFLOAT4(m_position.x, m_position.y, m_position.z, 1.0f);
	renderManager->SetConstant(RenderManager::CONSTANT_TYPE::CAMERA, &constant, sizeof(constant));

}
