//=====================================================================================
// BloomBrightPS.hlsl - 高輝度抽出ピクセルシェーダー
// Author:Kaito Aoki
//=====================================================================================

#include "Common.hlsl"

// Material (b3) を転用してポストプロセスパラメータを渡す
// Material.BaseColor.x = Threshold (高輝度閾値)
// Material.BaseColor.y = SoftKnee (ソフトニー)
// Material.BaseColor.z = Intensity (Bloom強度, Compositeでも使用)

PS_OUTPUT main(PS_IN input)
{
    PS_OUTPUT output;

    float4 color = TextureBaseColor.Sample(Sampler, input.TexCoord);
    
    // 輝度計算 (Rec.709)
    float luminance = dot(color.rgb, float3(0.2126f, 0.7152f, 0.0722f));
    
    float threshold = Material.BaseColor.x;
    float softKnee = Material.BaseColor.y;
    
    // ソフトニー付き閾値処理
    float knee = threshold * softKnee;
    float soft = luminance - threshold + knee;
    soft = clamp(soft, 0.0f, 2.0f * knee);
    soft = soft * soft / (4.0f * knee + 0.00001f);
    
    float contribution = max(soft, luminance - threshold);
    contribution /= max(luminance, 0.00001f);
    
    output.Color = float4(color.rgb * contribution, 1.0f);
    return output;
}
