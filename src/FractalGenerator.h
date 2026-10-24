//=====================================================================================
// FractalGenerator.h
// Author: KTEngine / Antigravity
// Description: Component to generate 3D fractal structures (Fractal Trees, Menger Sponge, etc.)
//              by duplicating ModelRenderer onto hierarchical child GameObjects.
//=====================================================================================

#ifndef _FRACTALGENERATOR_H_
#define _FRACTALGENERATOR_H_

#include "Component.h"
#include "ktvector.hpp"
#include <vector>
#include <string>
#include <cereal/types/vector.hpp>

class Shader;

enum class FractalPreset {
    Tree_Classic2D = 0,   // 2D 2-way planar tree
    Tree_Y3D       = 1,   // 3D 2-way alternating twist tree
    Tree_Trident3D = 2,   // 3D 3-way radial tree (120 deg)
    Custom         = 3,   // Fully customizable tree
    MengerSponge   = 4    // Menger Sponge 3D fractal
};

class FractalGenerator : public Component {
private:
    FractalPreset m_Preset = FractalPreset::Tree_Trident3D;

    int   m_MaxDepth       = 4;      // Recursion depth (1 - 6 for tree, 1 - 3 for sponge)
    int   m_BranchCount    = 3;      // Branches per node (2 - 5)
    float m_BranchAngle    = 35.0f;  // Branch tilt angle from parent axis (degrees)
    float m_TwistAngle     = 120.0f; // Azimuth twist angle per branch / depth (degrees)
    float m_ScaleDecay     = 0.70f;  // Scale ratio for children relative to parent
    float m_BranchOffsetY  = 1.0f;   // Local offset along Y to sprout branches
    bool  m_AutoRegenerate = false;  // Automatically regenerate on inspector change
    bool  m_InheritShader  = true;   // Inherit root GameObject's Shader & material settings

    // Menger Sponge parameters
    float m_SpongeSpacing  = 1.0f;   // Spacing scale for Menger sponge cubes
    bool  m_RootModelHidden = false; // Tracks if root ModelRenderer was hidden by sponge

    std::vector<uint32_t> m_SpawnedChildIds;

    void ApplyPreset(FractalPreset preset);
    void GenerateRecursive(GameObject* parent, int currentDepth, const std::string& modelPath, const Shader* rootShader);
    void GenerateMengerRecursive(GameObject* parent, int currentDepth, const std::string& modelPath, const Shader* rootShader);
    int  CalculateTotalObjectCount() const;

public:
    FractalGenerator();
    virtual ~FractalGenerator();

    void Awake() override;
    void OnDestroy() override;
    void OnDestroyOnEditor() override;
    void ShowUI() override;

    std::string GetComponentName() override { return "FractalGenerator"; }

    // Core generator API
    void Generate();
    void Clear();

    // Accessors
    FractalPreset GetPreset() const { return m_Preset; }
    void SetPreset(FractalPreset preset) { ApplyPreset(preset); }

    int GetMaxDepth() const { return m_MaxDepth; }
    void SetMaxDepth(int depth);

    float GetBranchAngle() const { return m_BranchAngle; }
    void SetBranchAngle(float angle) { m_BranchAngle = angle; }

    float GetScaleDecay() const { return m_ScaleDecay; }
    void SetScaleDecay(float decay) { m_ScaleDecay = decay; }

    bool GetInheritShader() const { return m_InheritShader; }
    void SetInheritShader(bool inherit) { m_InheritShader = inherit; }

    float GetSpongeSpacing() const { return m_SpongeSpacing; }
    void SetSpongeSpacing(float spacing) { m_SpongeSpacing = spacing; }

    template <class Archive>
    void serialize(Archive& ar) {
        ar(cereal::base_class<Component>(this));
        ar(cereal::make_nvp("Preset", m_Preset));
        ar(cereal::make_nvp("MaxDepth", m_MaxDepth));
        ar(cereal::make_nvp("BranchCount", m_BranchCount));
        ar(cereal::make_nvp("BranchAngle", m_BranchAngle));
        ar(cereal::make_nvp("TwistAngle", m_TwistAngle));
        ar(cereal::make_nvp("ScaleDecay", m_ScaleDecay));
        ar(cereal::make_nvp("BranchOffsetY", m_BranchOffsetY));
        ar(cereal::make_nvp("AutoRegenerate", m_AutoRegenerate));
        try {
            ar(cereal::make_nvp("InheritShader", m_InheritShader));
        } catch (...) {
            m_InheritShader = true;
        }
        try {
            ar(cereal::make_nvp("SpongeSpacing", m_SpongeSpacing));
        } catch (...) {
            m_SpongeSpacing = 1.0f;
        }
        try {
            ar(cereal::make_nvp("RootModelHidden", m_RootModelHidden));
        } catch (...) {
            m_RootModelHidden = false;
        }
        ar(cereal::make_nvp("SpawnedChildIds", m_SpawnedChildIds));
    }
};

#endif // !_FRACTALGENERATOR_H_
