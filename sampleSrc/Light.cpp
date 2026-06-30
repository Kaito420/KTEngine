// ======================================================================
// Light.cpp
// author: Kaito Aoki
// Date: 2026/05/11/
// ======================================================================

#include "Light.h"

Light::Light(){
	m_position = KTVECTOR3(0.0f, 10.0f, 2.0f);
	m_color = KTVECTOR4(1.0f, 1.0f, 1.0f, 1.0f);
	m_intensity = 1.0f;
	m_diffuseModel = 0;
	m_shadingModel = 0;
	m_specularModel = 0;
	m_rimLightModel = 0;
	m_rimPower = 1.0f;
	m_rimColor = KTVECTOR3(0.0f, 0.0f, 0.0f);
}

void Light::Update() {

}

void Light::Draw() {
	// SetConstantはDrawタイミングで呼ぶ必要がある
	RenderManager* renderManager = RenderManager::GetInstance();
	ENV_CONSTANT envConstant{};
	KTVECTOR3 normalizedDirection = m_position.Normalize();
	envConstant.LightDirection = XMFLOAT4(m_position.x, m_position.y, m_position.z, 0.0f);
	envConstant.LightColor = XMFLOAT4(m_color.x * m_intensity, m_color.y * m_intensity, m_color.z * m_intensity, m_color.w);
	envConstant.DiffuseModel = m_diffuseModel;
	envConstant.ShadingModel = m_shadingModel;
	envConstant.SpecularModel = m_specularModel;
	envConstant.RimLightModel = m_rimLightModel;
	envConstant.RimPower = m_rimPower;
	envConstant.RimColor = XMFLOAT3(m_rimColor.x, m_rimColor.y, m_rimColor.z);
	renderManager->SetConstant(RenderManager::CONSTANT_TYPE::ENV, &envConstant, sizeof(envConstant));
}