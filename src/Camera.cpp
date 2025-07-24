//=====================================================================================
// Camera.cpp
// Author:Kaito Aoki
// Date:2025/07/18
//=====================================================================================

#include "Camera.h"

void Camera::Awake() {
	//一旦初期化

}

void Camera::Update() {
	//ここでRendererDX11のViewとProjectionを更新する？
	XMMATRIX view = XMMatrixLookAtLH(
		XMVectorSet(0.0f, 0.0f, -10.0f, 0.0f), // カメラ位置
		XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f), // 注視点
		XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)  // 上方向
	);
	XMMATRIX projection = XMMatrixPerspectiveFovLH(XMConvertToRadians(60.0f), 1280.0f / 720.0f, 0.1f, 1000.0f);

	RendererDX11::SetViewMatrix(view);
	RendererDX11::SetProjectionMatrix(projection);
}
