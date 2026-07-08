#include "Common.hlsl"

PS_OUTPUT main(PS_IN input){
    PS_OUTPUT output;

    // 元のピクセルカラーをサンプリング
    float4 color = TextureBaseColor.Sample(Sampler, input.TexCoord);
    float3 rgb = color.rgb;

    // 1. Brightness（輝度）の調整
    rgb += Material.BaseColor.z;

    // 2. Contrast（コントラスト）の調整
    // 中間灰色を基準にスケール
    // Material.BaseColor.xにContrastを格納
    rgb = (rgb - 0.5) * Material.BaseColor.x + 0.5;

    // 3. Saturation（彩度）の調整
    // 輝度（Luminance）を算出してブレンド
    // Material.BaseColor.yにSaturationを格納
    float luminance = dot(rgb, float3(0.2126, 0.7152, 0.0722));
    rgb = lerp(float3(luminance, luminance, luminance), rgb, Material.BaseColor.y);

    // 4. Color Filter（カラーフィルター）の適用
    // Material.EmissionColor.rgbにColorFilterを格納
    rgb *= Material.EmissionColor.rgb;

    output.Color = float4(max(rgb, 0.0), color.a);
    return output;

}