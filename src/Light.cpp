//=====================================================================================
// Light.cpp
// Author:Kaito Aoki
// Date:2026/01/29
//=====================================================================================

#include "Light.h"
#include "imgui.h"

void Light::Awake(){
	_executeInEditor = true;	// エディターで実行
	_lightData.Enable = true;
	_lightData.Direction = XMFLOAT4(0.0f, -1.0f, -1.0f, 0.0f);
	_lightData.Ambient = XMFLOAT4(0.2f, 0.2f, 0.2f, 1.0f);
	_lightData.Diffuse = XMFLOAT4(0.8f, 0.8f, 0.8f, 1.0f);
	_lightData.Position = XMFLOAT4(-5.0f, 10.0f, 5.0f, 0.0f);
	_lightData.Parameter = XMFLOAT4(100.0f, 1.5f, 0.0f, 0.0f);

	_lightData.DiffuseModel = 0;
	_lightData.ShadingModel = 0;
	_lightData.SpecularModel = 1;
	_lightData.RimLightModel = 0;
	_lightData.RimPower = 2.0f;
	_lightData.RimColor = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);

	_lightData.Intensity = 1.5f;
	_lightData.AmbientIntensity = 1.0f;
	_lightData.Exposure = 1.0f;
}

void Light::Update(){
	Renderer::SetLight(_lightData);
}

void Light::ShowUI(){
	// Light settings UI
	ImGui::Checkbox("Enable Light", (bool*)&_lightData.Enable);
	
	ImGui::DragFloat("Light Intensity", &_lightData.Intensity, 0.05f, 0.0f, 10.0f, "%.2f");
	ImGui::DragFloat("Ambient Intensity", &_lightData.AmbientIntensity, 0.05f, 0.0f, 5.0f, "%.2f");
	ImGui::DragFloat("Exposure (Tone Mapping)", &_lightData.Exposure, 0.05f, 0.1f, 5.0f, "%.2f");

	float dir[3] = { _lightData.Direction.x, _lightData.Direction.y, _lightData.Direction.z };
	if (ImGui::DragFloat3("Direction", dir, 0.1f)) {
		_lightData.Direction = XMFLOAT4(dir[0], dir[1], dir[2], 0.0f);
	}
	
	float diff[4] = { _lightData.Diffuse.x, _lightData.Diffuse.y, _lightData.Diffuse.z, _lightData.Diffuse.w };
	if (ImGui::ColorEdit4("Diffuse Color", diff)) {
		_lightData.Diffuse = XMFLOAT4(diff[0], diff[1], diff[2], diff[3]);
	}
	
	float amb[4] = { _lightData.Ambient.x, _lightData.Ambient.y, _lightData.Ambient.z, _lightData.Ambient.w };
	if (ImGui::ColorEdit4("Ambient Color", amb)) {
		_lightData.Ambient = XMFLOAT4(amb[0], amb[1], amb[2], amb[3]);
	}
	
	float pos[3] = { _lightData.Position.x, _lightData.Position.y, _lightData.Position.z };
	if (ImGui::DragFloat3("Position", pos, 0.1f)) {
		_lightData.Position = XMFLOAT4(pos[0], pos[1], pos[2], 0.0f);
	}

	const char* diffuseModels[] = { "Lambert", "Half-Lambert", "Normalized-Lambert" };
	ImGui::Combo("Diffuse Model", &_lightData.DiffuseModel, diffuseModels, IM_ARRAYSIZE(diffuseModels));
	
	const char* shadingModels[] = { "Smooth", "Toon" };
	ImGui::Combo("Shading Style", &_lightData.ShadingModel, shadingModels, IM_ARRAYSIZE(shadingModels));
	
	const char* specularModels[] = { "Off", "Phong/BRDF Specular" };
	ImGui::Combo("Specular Model", &_lightData.SpecularModel, specularModels, IM_ARRAYSIZE(specularModels));
	
	const char* rimLightModels[] = { "Off", "On" };
	ImGui::Combo("Rim Light Model", &_lightData.RimLightModel, rimLightModels, IM_ARRAYSIZE(rimLightModels));
	
	if (_lightData.RimLightModel != 0) {
		ImGui::DragFloat("Rim Power", &_lightData.RimPower, 0.1f, 0.0f, 16.0f);
		float rimCol[4] = { _lightData.RimColor.x, _lightData.RimColor.y, _lightData.RimColor.z, _lightData.RimColor.w };
		if (ImGui::ColorEdit4("Rim Color", rimCol)) {
			_lightData.RimColor = XMFLOAT4(rimCol[0], rimCol[1], rimCol[2], rimCol[3]);
		}
	}
}
