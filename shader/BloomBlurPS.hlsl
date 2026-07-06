//=====================================================================================
// BloomBlurPS.hlsl - ガウシアンブラーピクセルシェーダー
// Author:Kaito Aoki
//=====================================================================================

#include "Common.hlsl"

// Material (b3) を転用
// Material.BaseColor.x = texelSizeX (1.0 / textureWidth)
// Material.BaseColor.y = texelSizeY (1.0 / textureHeight)
// Material.BaseColor.z = isHorizontal (1.0 = 水平, 0.0 = 垂直)

// 13タップ ガウシアンカーネル (σ ≈ 4.0)
static const int KERNEL_SIZE = 7; // 片側7サンプル（中心含めて13タップ）
static const float weights[7] = {
    0.1964825501511404f,
    0.2969069646728344f,
    0.2195956084680983f,
    0.0448831857968421f,
    0.0105399224561864f,
    0.0015408050889339f,
    0.0000886311102653f
};

// 最適化: リニアサンプリングでテクスチャフェッチ回数を削減
static const float offsets[7] = {
    0.0f, 1.4117647058823530f, 3.2941176470588234f,
    5.1764705882352940f, 7.0588235294117645f,
    8.9411764705882355f, 10.8235294117647060f
};

PS_OUTPUT main(PS_IN input)
{
    PS_OUTPUT output;

    float2 texelSize = float2(Material.BaseColor.x, Material.BaseColor.y);
    bool isHorizontal = Material.BaseColor.z > 0.5f;
    
    float2 direction = isHorizontal ? float2(1.0f, 0.0f) : float2(0.0f, 1.0f);
    
    // 中心ピクセル
    float3 result = TextureBaseColor.Sample(Sampler, input.TexCoord).rgb * weights[0];
    
    // 両側のサンプル
    [unroll]
    for (int i = 1; i < KERNEL_SIZE; i++)
    {
        float2 offset = direction * texelSize * offsets[i];
        result += TextureBaseColor.Sample(Sampler, input.TexCoord + offset).rgb * weights[i];
        result += TextureBaseColor.Sample(Sampler, input.TexCoord - offset).rgb * weights[i];
    }

    output.Color = float4(result, 1.0f);
    return output;
}
