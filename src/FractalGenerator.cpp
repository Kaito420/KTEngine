//=====================================================================================
// FractalGenerator.cpp
// Author: KTEngine / Antigravity
// Description: Implementation of FractalGenerator component.
//=====================================================================================

#include "FractalGenerator.h"
#include "ModelRenderer.h"
#include "Shader.h"
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

void FractalGenerator::SetMaxDepth(int depth) {
    if (m_Preset == FractalPreset::MengerSponge) {
        m_MaxDepth = (depth < 1) ? 1 : ((depth > 3) ? 3 : depth);
    } else {
        m_MaxDepth = (depth < 1) ? 1 : ((depth > 6) ? 6 : depth);
    }
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

    case FractalPreset::MengerSponge:
        if (m_MaxDepth > 3 || m_MaxDepth < 1) {
            m_MaxDepth = 2; // Default to depth 2 (400 cubes) for immediate response
        }
        m_ScaleDecay    = 1.0f / 3.0f;
        m_SpongeSpacing = 1.0f;
        break;
    }
}

int FractalGenerator::CalculateTotalObjectCount() const {
    if (m_Preset == FractalPreset::MengerSponge) {
        int depth = (m_MaxDepth < 1) ? 1 : ((m_MaxDepth > 3) ? 3 : m_MaxDepth);
        int leaves = 1;
        for (int d = 0; d < depth; d++) {
            leaves *= 20;
        }
        return leaves;
    }

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

    // Restore root ModelRenderer visibility if it was hidden by Menger Sponge
    if (m_RootModelHidden) {
        auto mr = _owner->GetComponent<ModelRenderer>();
        if (mr) {
            mr->Active(true);
        }
        m_RootModelHidden = false;
    }

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

    // 1. Clear existing spawned branches / sponges
    Clear();

    // 2. Identify ModelRenderer on owner
    std::string modelPath = "";
    auto mr = _owner->GetComponent<ModelRenderer>();
    if (mr && mr->GetModel()) {
        modelPath = mr->GetLoadedFileName();
    }

    // 3. Identify Shader on owner
    const Shader* rootShader = _owner->GetComponent<Shader>();

    // 4. Begin generation according to preset
    if (m_Preset == FractalPreset::MengerSponge) {
        // Temporarily hide the root solid cube so internal holes are visible
        if (mr && mr->GetActive()) {
            mr->Active(false);
            m_RootModelHidden = true;
        }
        GenerateMengerRecursive(_owner, 1, modelPath, rootShader);
    } else {
        GenerateRecursive(_owner, 1, modelPath, rootShader);
    }
}

void FractalGenerator::GenerateRecursive(GameObject* parent, int currentDepth, const std::string& modelPath, const Shader* rootShader) {
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

        // Inherit parent's Shader & Material if enabled
        if (m_InheritShader && rootShader) {
            auto childShader = child->GetComponent<Shader>();
            if (!childShader) {
                childShader = child->AddComponent<Shader>();
            }
            if (childShader) {
                childShader->CopyFrom(rootShader);
            }
        }

        m_SpawnedChildIds.push_back(child->_id);

        // Recurse to next depth level
        if (currentDepth < m_MaxDepth) {
            GenerateRecursive(child, currentDepth + 1, modelPath, rootShader);
        }
    }
}

void FractalGenerator::GenerateMengerRecursive(GameObject* parent, int currentDepth, const std::string& modelPath, const Shader* rootShader) {
    if (!parent || currentDepth > m_MaxDepth) return;
    auto scene = Manager::GetCurrentScene();
    if (!scene) return;

    // Subdivide parent into 3x3x3 grid; step between sub-cubes
    float step = (1.0f / 3.0f) * m_SpongeSpacing;
    float scale = 1.0f / 3.0f;

    for (int x = -1; x <= 1; x++) {
        for (int y = -1; y <= 1; y++) {
            for (int z = -1; z <= 1; z++) {
                // Menger sponge rule: remove center holes (points with 2 or more zeros)
                int zeros = (x == 0 ? 1 : 0) + (y == 0 ? 1 : 0) + (z == 0 ? 1 : 0);
                if (zeros >= 2) continue; // 7 removed cubes, 20 remaining cubes

                GameObject* child = scene->AddGameObject<GameObject>();
                child->_name = "Fractal_M_D" + std::to_string(currentDepth) + 
                               "_" + std::to_string(x) + "_" + std::to_string(y) + "_" + std::to_string(z);

                // Attach as child with local transform
                child->SetParent(parent->shared_from_this(), false);
                child->_transform._position   = KTVECTOR3((float)x * step, (float)y * step, (float)z * step);
                child->_transform._quaternion = KTQUATERNION(0.0f, 0.0f, 0.0f, 1.0f);
                child->_transform._rotation   = KTVECTOR3(0.0f, 0.0f, 0.0f);
                child->_transform._scale      = KTVECTOR3(scale, scale, scale);

                // Only attach ModelRenderer & Shader to leaf nodes (final depth level)
                // This ensures interior cavities are not occluded by parent solid geometry
                if (currentDepth == m_MaxDepth) {
                    if (!modelPath.empty()) {
                        auto childMr = child->AddComponent<ModelRenderer>();
                        childMr->Load(modelPath.c_str());
                    }

                    if (m_InheritShader && rootShader) {
                        auto childShader = child->GetComponent<Shader>();
                        if (!childShader) {
                            childShader = child->AddComponent<Shader>();
                        }
                        if (childShader) {
                            childShader->CopyFrom(rootShader);
                        }
                    }
                }

                m_SpawnedChildIds.push_back(child->_id);

                // Recurse to next depth level
                if (currentDepth < m_MaxDepth) {
                    GenerateMengerRecursive(child, currentDepth + 1, modelPath, rootShader);
                }
            }
        }
    }
}

void FractalGenerator::ShowUI() {
    bool changed = false;

    // Check ModelRenderer on owner
    auto mr = _owner ? _owner->GetComponent<ModelRenderer>() : nullptr;
    if (!mr || !mr->GetModel()) {
        ImGui::TextColored(ImVec4(1.0f, 0.65f, 0.2f, 1.0f), "Notice: No loaded ModelRenderer on this GameObject.");
        ImGui::TextDisabled("Branches/cubes will generate without 3D models unless ModelRenderer is loaded.");
        ImGui::Separator();
    } else {
        std::string fname = mr->GetLoadedFileName();
        ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.5f, 1.0f), "Using Model: %s", fname.c_str());
        ImGui::Separator();
    }

    // Check Shader on owner & Inherit toggle
    auto shader = _owner ? _owner->GetComponent<Shader>() : nullptr;
    if (shader) {
        changed |= ImGui::Checkbox("Inherit Parent Shader / Material", &m_InheritShader);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Copy parent object's Shader, BaseColor, PBR material parameters, and textures to all branches/cubes.");
        }
        ImGui::Separator();
    }

    // Preset selection
    const char* presetNames[] = {
        "Fractal Tree (2D Planar)",
        "Fractal Tree (3D Alternating Y)",
        "Fractal Tree (3D Radial Trident)",
        "Custom Tree Parameters",
        "Menger Sponge (3D Subdivision)"
    };
    int currentPresetIndex = static_cast<int>(m_Preset);
    if (ImGui::Combo("Preset", &currentPresetIndex, presetNames, IM_COUNTOF(presetNames))) {
        ApplyPreset(static_cast<FractalPreset>(currentPresetIndex));
        changed = true;
    }

    ImGui::Spacing();

    if (m_Preset == FractalPreset::MengerSponge) {
        // Menger Sponge Controls
        changed |= ImGui::SliderInt("Recursion Depth", &m_MaxDepth, 1, 3);
        if (m_MaxDepth == 3) {
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Warning: Depth 3 generates 8,000 cubes (high drawcall load)!");
        }

        changed |= ImGui::SliderFloat("Cube Spacing", &m_SpongeSpacing, 0.80f, 1.50f, "%.2f");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("1.0 = standard touching cubes. >1.0 introduces gaps between cubes.");
        }

        ImGui::TextDisabled("Rule: 3x3x3 grid minus 7 cross cubes = 20 cubes per level.");
        ImGui::TextDisabled("Root/intermediate cubes hidden; only leaf cubes rendered.");
    } else {
        // Tree Controls
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
    }

    ImGui::Spacing();

    // Stats & Preview
    int totalCount = CalculateTotalObjectCount();
    if (m_Preset == FractalPreset::MengerSponge) {
        ImGui::TextDisabled("Estimated Leaf Cubes: %d objects", totalCount);
    } else {
        ImGui::TextDisabled("Estimated Branch Count: %d objects", totalCount);
    }
    if (!m_SpawnedChildIds.empty()) {
        ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "Active Generated Objects: %d", (int)m_SpawnedChildIds.size());
    }

    ImGui::Separator();

    // Action Buttons
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.3f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.75f, 0.38f, 1.0f));
    const char* genButtonLabel = (m_Preset == FractalPreset::MengerSponge) ? "Generate Sponge" : "Generate Tree";
    if (ImGui::Button(genButtonLabel, ImVec2(140, 28))) {
        Generate();
    }
    ImGui::PopStyleColor(2);

    ImGui::SameLine();

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.25f, 0.25f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.75f, 0.3f, 0.3f, 1.0f));
    if (ImGui::Button("Clear", ImVec2(100, 28))) {
        Clear();
    }
    ImGui::PopStyleColor(2);

    ImGui::Checkbox("Auto Regenerate", &m_AutoRegenerate);
    if (m_AutoRegenerate && changed) {
        // Safe auto-regenerate thresholds to avoid UI freezing
        if (m_Preset == FractalPreset::MengerSponge && m_MaxDepth <= 2) {
            Generate();
        } else if (m_Preset != FractalPreset::MengerSponge && m_MaxDepth <= 4) {
            Generate();
        }
    }
}
