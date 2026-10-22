//=====================================================================================
// FractalGenerator.cpp
// Author: KTEngine / Antigravity
// Description: Implementation of FractalGenerator component.
//=====================================================================================

#include "FractalGenerator.h"
#include "ModelRenderer.h"
#include "Manager.h"
#include "Scene.h"
#include "imgui.h"
#include <cmath>

FractalGenerator::FractalGenerator() {
    ApplyPreset(m_Preset);
}

FractalGenerator::~FractalGenerator() {
}

void FractalGenerator::Awake() {
}

void FractalGenerator::OnDestroy() {
    Clear();
}

void FractalGenerator::OnDestroyOnEditor() {
    Clear();
}

void FractalGenerator::ApplyPreset(FractalPreset preset) {
    m_Preset = preset;
    switch (preset) {
    case FractalPreset::Tree_Classic2D:
        m_MaxDepth      = 4;
        m_BranchCount   = 2;
        m_BranchAngle   = 30.0f;
        m_TwistAngle    = 0.0f;
        m_ScaleDecay    = 0.75f;
        m_BranchOffsetY = 1.0f;
        break;

    case FractalPreset::Tree_Y3D:
        m_MaxDepth      = 4;
        m_BranchCount   = 2;
        m_BranchAngle   = 30.0f;
        m_TwistAngle    = 90.0f;
        m_ScaleDecay    = 0.75f;
        m_BranchOffsetY = 1.0f;
        break;

    case FractalPreset::Tree_Trident3D:
        m_MaxDepth      = 4;
        m_BranchCount   = 3;
        m_BranchAngle   = 35.0f;
        m_TwistAngle    = 120.0f;
        m_ScaleDecay    = 0.70f;
        m_BranchOffsetY = 1.0f;
        break;

    case FractalPreset::Custom:
        // Keep current values
        break;
    }
}

int FractalGenerator::CalculateTotalObjectCount() const {
    if (m_BranchCount <= 1) return m_MaxDepth;
    // Sum of geometric series: r + r^2 + ... + r^n = r * (r^n - 1) / (r - 1)
    double r = (double)m_BranchCount;
    double n = (double)m_MaxDepth;
    double count = (r * (std::pow(r, n) - 1.0)) / (r - 1.0);
    if (count > 50000.0) return 50000;
    return (int)count;
}

void FractalGenerator::Clear() {
    if (!_owner) return;
    auto scene = Manager::GetCurrentScene();
    if (!scene) return;

    // Destroy all spawned children recursively
    std::vector<std::shared_ptr<GameObject>> childrenToDestroy;
    std::function<void(const std::shared_ptr<GameObject>&)> collectFractalChildren =
        [&](const std::shared_ptr<GameObject>& parent) {
            if (!parent) return;
            for (auto& child : parent->GetChildren()) {
                if (!child) continue;
                bool isSpawned = false;
                for (uint32_t id : m_SpawnedChildIds) {
                    if (child->_id == id) {
                        isSpawned = true;
                        break;
                    }
                }
                if (!isSpawned && child->_name.rfind("Fractal_", 0) == 0) {
                    isSpawned = true;
                }

                if (isSpawned) {
                    childrenToDestroy.push_back(child);
                }
                collectFractalChildren(child);
            }
        };

    collectFractalChildren(_owner->shared_from_this());

    for (auto& child : childrenToDestroy) {
        if (child && !child->IsDestroy()) {
            child->Destroy();
        }
    }

    m_SpawnedChildIds.clear();
}

void FractalGenerator::Generate() {
    if (!_owner) return;
    auto scene = Manager::GetCurrentScene();
    if (!scene) return;

    // 1. Clear existing spawned branches
    Clear();

    // 2. Identify ModelRenderer on owner
    std::string modelPath = "";
    auto mr = _owner->GetComponent<ModelRenderer>();
    if (mr && mr->GetModel()) {
        modelPath = mr->GetLoadedFileName();
    }

    // 3. Begin recursive branch generation
    GenerateRecursive(_owner, 1, modelPath);
}

void FractalGenerator::GenerateRecursive(GameObject* parent, int currentDepth, const std::string& modelPath) {
    if (!parent || currentDepth > m_MaxDepth) return;
    auto scene = Manager::GetCurrentScene();
    if (!scene) return;

    int branchCount = (m_Preset == FractalPreset::Tree_Classic2D || m_Preset == FractalPreset::Tree_Y3D) ? 2 : m_BranchCount;
    if (branchCount < 1) branchCount = 1;

    for (int i = 0; i < branchCount; i++) {
        KTQUATERNION branchRot;

        if (m_Preset == FractalPreset::Tree_Classic2D) {
            // 2D planar branching along Z-axis tilt
            float tilt = (i == 0) ? m_BranchAngle : -m_BranchAngle;
            branchRot = KTQUATERNION::FromAxisAngle(KTVECTOR3(0.0f, 0.0f, 1.0f), tilt);
        }
        else if (m_Preset == FractalPreset::Tree_Y3D) {
            // 3D 2-way alternating twist
            float twist = (float)(currentDepth - 1) * m_TwistAngle;
            float tilt  = (i == 0) ? m_BranchAngle : -m_BranchAngle;
            KTQUATERNION rotYaw = KTQUATERNION::FromAxisAngle(KTVECTOR3(0.0f, 1.0f, 0.0f), twist);
            KTQUATERNION rotPitch = KTQUATERNION::FromAxisAngle(KTVECTOR3(1.0f, 0.0f, 0.0f), tilt);
            branchRot = (rotYaw * rotPitch).Normalize();
        }
        else {
            // Radial branching (Trident3D or Custom)
            float stepYaw = 360.0f / (float)branchCount;
            float yaw   = stepYaw * (float)i + (float)(currentDepth - 1) * m_TwistAngle;
            float pitch = m_BranchAngle;

            KTQUATERNION rotYaw = KTQUATERNION::FromAxisAngle(KTVECTOR3(0.0f, 1.0f, 0.0f), yaw);
            KTQUATERNION rotPitch = KTQUATERNION::FromAxisAngle(KTVECTOR3(1.0f, 0.0f, 0.0f), pitch);
            branchRot = (rotYaw * rotPitch).Normalize();
        }

        // Create child GameObject
        GameObject* child = scene->AddGameObject<GameObject>();
        child->_name = "Fractal_D" + std::to_string(currentDepth) + "_B" + std::to_string(i);

        // Attach as child with local transform
        child->SetParent(parent->shared_from_this(), false);
        child->_transform._position   = KTVECTOR3(0.0f, m_BranchOffsetY, 0.0f);
        child->_transform._quaternion = branchRot;
        child->_transform._rotation   = branchRot.ToEulerAngles();
        child->_transform._scale      = KTVECTOR3(m_ScaleDecay, m_ScaleDecay, m_ScaleDecay);

        // Duplicate ModelRenderer if model exists
        if (!modelPath.empty()) {
            auto childMr = child->AddComponent<ModelRenderer>();
            childMr->Load(modelPath.c_str());
        }

        m_SpawnedChildIds.push_back(child->_id);

        // Recurse to next depth level
        if (currentDepth < m_MaxDepth) {
            GenerateRecursive(child, currentDepth + 1, modelPath);
        }
    }
}

void FractalGenerator::ShowUI() {
    bool changed = false;

    // Check ModelRenderer on owner
    auto mr = _owner ? _owner->GetComponent<ModelRenderer>() : nullptr;
    if (!mr || !mr->GetModel()) {
        ImGui::TextColored(ImVec4(1.0f, 0.65f, 0.2f, 1.0f), "Notice: No loaded ModelRenderer on this GameObject.");
        ImGui::TextDisabled("Branches will generate without 3D models unless ModelRenderer is loaded.");
        ImGui::Separator();
    } else {
        std::string fname = mr->GetLoadedFileName();
        ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.5f, 1.0f), "Using Model: %s", fname.c_str());
        ImGui::Separator();
    }

    // Preset selection
    const char* presetNames[] = {
        "Fractal Tree (2D Planar)",
        "Fractal Tree (3D Alternating Y)",
        "Fractal Tree (3D Radial Trident)",
        "Custom Parameters"
    };
    int currentPresetIndex = static_cast<int>(m_Preset);
    if (ImGui::Combo("Preset", &currentPresetIndex, presetNames, IM_COUNTOF(presetNames))) {
        ApplyPreset(static_cast<FractalPreset>(currentPresetIndex));
        changed = true;
    }

    ImGui::Spacing();

    // Max Depth Slider (Clamped 1 to 6 for performance safety)
    changed |= ImGui::SliderInt("Max Depth", &m_MaxDepth, 1, 6);
    if (m_MaxDepth >= 5) {
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "High depth generates many objects!");
    }

    // Branch Count (Editable only in Custom mode)
    if (m_Preset == FractalPreset::Custom) {
        changed |= ImGui::SliderInt("Branch Count", &m_BranchCount, 2, 5);
    } else {
        int displayCount = (m_Preset == FractalPreset::Tree_Trident3D) ? 3 : 2;
        ImGui::BeginDisabled();
        ImGui::SliderInt("Branch Count", &displayCount, 2, 5);
        ImGui::EndDisabled();
    }

    // Geometric Parameters
    changed |= ImGui::SliderFloat("Branch Tilt Angle", &m_BranchAngle, 0.0f, 85.0f, "%.1f deg");
    if (m_Preset != FractalPreset::Tree_Classic2D) {
        changed |= ImGui::SliderFloat("Twist Angle", &m_TwistAngle, -180.0f, 180.0f, "%.1f deg");
    }
    changed |= ImGui::SliderFloat("Scale Decay", &m_ScaleDecay, 0.40f, 0.85f, "%.2f");
    changed |= ImGui::DragFloat("Branch Offset Y", &m_BranchOffsetY, 0.05f, 0.1f, 10.0f, "%.2f");

    ImGui::Spacing();

    // Stats & Preview
    int totalCount = CalculateTotalObjectCount();
    ImGui::TextDisabled("Estimated Branch Count: %d objects", totalCount);
    if (!m_SpawnedChildIds.empty()) {
        ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "Active Generated Branches: %d", (int)m_SpawnedChildIds.size());
    }

    ImGui::Separator();

    // Action Buttons
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.3f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.75f, 0.38f, 1.0f));
    if (ImGui::Button("Generate Tree", ImVec2(140, 28))) {
        Generate();
    }
    ImGui::PopStyleColor(2);

    ImGui::SameLine();

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.25f, 0.25f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.75f, 0.3f, 0.3f, 1.0f));
    if (ImGui::Button("Clear Branches", ImVec2(120, 28))) {
        Clear();
    }
    ImGui::PopStyleColor(2);

    ImGui::Checkbox("Auto Regenerate", &m_AutoRegenerate);
    if (m_AutoRegenerate && changed && m_MaxDepth <= 4) {
        Generate();
    }
}

