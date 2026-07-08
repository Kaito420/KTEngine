//=====================================================================================
// PostProcessSystem.h
// Author:Kaito Aoki
// ポストプロセス設定管理・エディタUI
//=====================================================================================

#ifndef _POSTPROCESSSYSTEM_H_
#define _POSTPROCESSSYSTEM_H_

#include <cereal/cereal.hpp>

struct PostProcessSettings {
    // Bloom
    bool  BloomEnabled   = false;
    float BloomThreshold = 0.8f;
    float BloomSoftKnee  = 0.5f;
    float BloomIntensity = 1.0f;

    // Color Grading
    bool ColorGradingEnabled = false;
    float Contrast = 1.0f;      // コントラスト(0.5~2.0)
    float Saturation = 1.0f;    // 彩度(0.0~2.0)
    float Brightness = 0.0f;    // 輝度(-1.0~1.0)
    float ColorFilter[3] = {1.0f, 1.0f, 1.0f}; // カラーフィルター (RGB)

    // Depth of Field
    bool DofEnabled = false;
    float DofFocusDistance = 5.0f;
    float DofFocusRange = 3.0f;
    float DofBlurIntensity = 1.0f;

    template <class Archive>
    void serialize(Archive& ar){
        ar(
            cereal::make_nvp("BloomEnabled", BloomEnabled),
            cereal::make_nvp("BloomThreshold", BloomThreshold),
            cereal::make_nvp("BloomSoftKnee", BloomSoftKnee),
            cereal::make_nvp("BloomIntensity", BloomIntensity),
            cereal::make_nvp("ColorGradingEnabled", ColorGradingEnabled),
            cereal::make_nvp("Contrast", Contrast),
            cereal::make_nvp("Saturation", Saturation),
            cereal::make_nvp("Brightness", Brightness),
            cereal::make_nvp("ColorFilterR", ColorFilter[0]),
            cereal::make_nvp("ColorFilterG", ColorFilter[1]),
            cereal::make_nvp("ColorFilterB", ColorFilter[2]),
            cereal::make_nvp("DofEnabled", DofEnabled),
            cereal::make_nvp("DofFocusDistance", DofFocusDistance),
            cereal::make_nvp("DofFocusRange", DofFocusRange),
            cereal::make_nvp("DofBlurIntensity", DofBlurIntensity)
        );
    }
};

class PostProcessSystem {
public:
    static PostProcessSettings& GetSettings();
    static void RenderUI();
};

#endif // !_POSTPROCESSSYSTEM_H_
