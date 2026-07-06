//=====================================================================================
// BloomCompositePS.hlsl - Bloom合成ピクセルシェーダー
// Author:Kaito Aoki
//=====================================================================================

#include "Common.hlsl"

// t0: 元のシーンカラー (TextureBaseColor)
// t1: ブラー済みBloomテクスチャ (TextureNormal を転用)

// Material (b3) を転用
// Material.BaseColor.x = BloomIntensity

SamplerState LinearSampler : register(s1);

PS_OUTPUT main(PS_IN input)
{
    PS_OUTPUT output;

    float4 sceneColor = TextureBaseColor.Sample(Sampler, input.TexCoord);
    float4 bloomColor = TextureNormal.Sample(LinearSampler, input.TexCoord);
    
    float intensity = Material.BaseColor.x;
    
    // 加算合成
    float3 finalColor = sceneColor.rgb + bloomColor.rgb * intensity;
    
    output.Color = float4(finalColor, sceneColor.a);
    return output;
}
