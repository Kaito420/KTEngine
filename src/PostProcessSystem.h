//=====================================================================================
// PostProcessSystem.h
// Author:Kaito Aoki
// ポストプロセス設定管理・エディタUI
//=====================================================================================

#ifndef _POSTPROCESSSYSTEM_H_
#define _POSTPROCESSSYSTEM_H_

struct PostProcessSettings {
    // Bloom
    bool  BloomEnabled   = true;
    float BloomThreshold = 0.8f;
    float BloomSoftKnee  = 0.5f;
    float BloomIntensity = 1.0f;

    // 将来のエフェクト用
    // bool  VignetteEnabled = false;
    // float VignetteIntensity = 0.5f;
    // float VignetteRadius = 0.8f;
};

class PostProcessSystem {
public:
    static PostProcessSettings& GetSettings();
    static void RenderUI();
};

#endif // !_POSTPROCESSSYSTEM_H_
