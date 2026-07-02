

#include "Main.h"
#include "GameManager.h"

#include "ImGuiLayer.h"
#include "imgui.h"

GameManager* GameManager::m_Instance = nullptr;





GameManager::GameManager()
{
	m_Instance = this;
	m_Objects.push_back(&m_Camera);
	m_Objects.push_back(&m_Light);
	m_Objects.push_back(&m_Field);
	m_Objects.push_back(&m_Torus);

	//m_Objects.push_back(&m_Polygon2D);
}




GameManager::~GameManager()
{
	m_RenderManger.WaitGPU();
}





void GameManager::Update()
{
	{// Object
		for(auto obj: m_Objects)
			obj->Update();
	}
}




void GameManager::Draw()
{

	m_RenderManger.DrawBegin();


	{// Object
		for(auto obj: m_Objects)
			obj->Draw();
	}

	{
		// ヒエラルキーウィンドウ
		ImGui::Begin("Hierarchy");
		for (auto obj : m_Objects)
		{
			ImGui::Text(obj->GetName().c_str());
		}
		ImGui::End();

		//ImGui::ShowDemoWindow();
		ImGui::Begin("Camera Settings");
		{
			ImGui::DragFloat3("Position", &m_Camera.m_position.x, 0.1f);
			ImGui::DragFloat3("Rotation", &m_Camera.m_rotation.x, 0.1f);
		}
		ImGui::End();

		ImGui::Begin("Light Settings");
		{
			ImGui::DragFloat3("Position", &m_Light.m_position.x, 0.1f);
			//カラーピッカーで設定できるようにする
			ImGui::ColorPicker4("Color", &m_Light.m_color.x);
			ImGui::DragFloat("Intensity", &m_Light.m_intensity, 0.1f, 0.0f, 10.0f);

			// ライティングモデルの選択
			const char* models[] = { "Lambert", "Half-Lambert", "Normalized-Lambert" };
			ImGui::Combo("Diffuse Model", &m_Light.m_diffuseModel, models, IM_ARRAYSIZE(models));
			const char* shadingSyels[] = { "Smooth", "Toon" };
			ImGui::Combo("Shading Style", &m_Light.m_shadingModel, shadingSyels, IM_ARRAYSIZE(shadingSyels));
			const char* specularModels[] = { "Off", "Phong" };
			ImGui::Combo("Specular Model", &m_Light.m_specularModel, specularModels, IM_ARRAYSIZE(specularModels));
			const char* rimLightModels[] = { "Off", "On" };
			ImGui::Combo("Rim Light Model", &m_Light.m_rimLightModel, rimLightModels, IM_ARRAYSIZE(rimLightModels));
			if(m_Light.m_rimLightModel != 0) {
				ImGui::DragFloat("Rim Power", &m_Light.m_rimPower, 0.1f, 0.0f, 16.0f);
				ImGui::ColorEdit3("Rim Color", &m_Light.m_rimColor.x);
			}
		}
		ImGui::End();
	}


	m_RenderManger.DrawEnd();

}


