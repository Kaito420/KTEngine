//=====================================================================================
// PostProcessSystem.cpp
// Author:Kaito Aoki
// ポストプロセス設定管理・エディタUI
//=====================================================================================

#include "PostProcessSystem.h"
#include "imgui.h"

static PostProcessSettings s_settings;

PostProcessSettings& PostProcessSystem::GetSettings() {
    return s_settings;
}

void PostProcessSystem::RenderUI() {
    ImGui::Begin("Post Process");

    // Bloom セクション
    if (ImGui::CollapsingHeader("Bloom", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Enabled##Bloom", &s_settings.BloomEnabled);

        if (s_settings.BloomEnabled) {
            ImGui::SliderFloat("Threshold", &s_settings.BloomThreshold, 0.0f, 2.0f, "%.2f");
            ImGui::SliderFloat("Soft Knee", &s_settings.BloomSoftKnee, 0.0f, 1.0f, "%.2f");
            ImGui::SliderFloat("Intensity", &s_settings.BloomIntensity, 0.0f, 5.0f, "%.2f");
        }
    }

    // Color Grading セクション
    if (ImGui::CollapsingHeader("Color Grading", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Enabled##ColorGrading", &s_settings.ColorGradingEnabled);

        if (s_settings.ColorGradingEnabled) {
            ImGui::SliderFloat("Contrast", &s_settings.Contrast, 0.0f, 2.0f, "%.2f");
            ImGui::SliderFloat("Saturation", &s_settings.Saturation, 0.0f, 2.0f, "%.2f");
            ImGui::SliderFloat("Brightness", &s_settings.Brightness, -1.0f, 1.0f, "%.2f");
            ImGui::ColorEdit3("Color Filter", s_settings.ColorFilter);
        }
    }

    // 将来のエフェクトセクション
    // if (ImGui::CollapsingHeader("Vignette")) { ... }
    // if (ImGui::CollapsingHeader("Color Grading")) { ... }

    ImGui::End();
}
