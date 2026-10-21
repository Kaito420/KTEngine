//=====================================================================================
// Camera.cpp
// Author:Kaito Aoki
// Date:2025/07/18
//=====================================================================================

#include "Camera.h"
#include "Input.h"
#include "Manager.h"
#include <Windows.h>

void Camera::Awake() {
	_executeInEditor = true;	//�G�f�B�^���[�h�ł���s����
}

void Camera::Update() {
	//������RendererDX11��View��Projection��X�V����

	//���W�A���ɕϊ�
	float pitch = XMConvertToRadians(_transform._rotation.x);
	float yaw = XMConvertToRadians(_transform._rotation.y);
	float roll = XMConvertToRadians(_transform._rotation.z);

	//��]�s��쐬
	XMMATRIX rotMtx = XMMatrixRotationRollPitchYaw(pitch, yaw, roll);

	//�x�N�g���ɕϊ�
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

	KTVECTOR3 worldPos = GetWorldPosition();
	XMVECTOR position = XMVectorSet(worldPos.x, worldPos.y, worldPos.z, 1.0f);

	XMMATRIX view = XMMatrixLookAtLH(
		position, // �J�����ʒu
		position + frontVec * _distance, // �����_
		upVec  // �����
	);

	//RendererDX11����V�[���T�C�Y��擾���ăA�X�y�N�g���v�Z
	float width = Renderer::GetGameWidth();
	float height = Renderer::GetGameHeight();
	float aspectRatio = (height > 0.0f) ? (width / height) : Manager::GetAspectRatioValue();

	XMMATRIX projection = XMMatrixPerspectiveFovLH(
		XMConvertToRadians(60.0f),
		aspectRatio,
		0.1f,
		1000.0f);

	_viewMatrix = view;
	_projectionMatrix = projection;
}
