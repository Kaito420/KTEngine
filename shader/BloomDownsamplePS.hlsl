//=====================================================================================
// BloomDownsamplePS.hlsl - ダウンサンプルピクセルシェーダー
// Author:Kaito Aoki
//=====================================================================================

#include "Common.hlsl"

// Material (b3) を転用
// Material.BaseColor.x = texelSizeX (1.0 / sourceWidth)
// Material.BaseColor.y = texelSizeY (1.0 / sourceHeight)

PS_OUTPUT main(PS_IN input)
{
    PS_OUTPUT output;

    float2 texelSize = float2(Material.BaseColor.x, Material.BaseColor.y);
    float2 uv = input.TexCoord;

    // 4テクセル ボックスフィルタ（バイリニアサンプリング活用で4回フェッチ）
    float3 color = float3(0.0f, 0.0f, 0.0f);
    color += TextureBaseColor.Sample(Sampler, uv + float2(-0.5f, -0.5f) * texelSize).rgb;
    color += TextureBaseColor.Sample(Sampler, uv + float2( 0.5f, -0.5f) * texelSize).rgb;
    color += TextureBaseColor.Sample(Sampler, uv + float2(-0.5f,  0.5f) * texelSize).rgb;
    color += TextureBaseColor.Sample(Sampler, uv + float2( 0.5f,  0.5f) * texelSize).rgb;
    color *= 0.25f;

    output.Color = float4(color, 1.0f);
    return output;
}
