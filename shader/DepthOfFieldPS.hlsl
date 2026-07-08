#include "Common.hlsl"

// 定数バッファの転用（Material.BaseColor)
// Material.BaseColor.x = FocusDistance
// Material.BaseColor.y = FocusRange
// Material.BaseColor.z = BlurIntensity

// テクスチャレジスタの割り当て
// register(t0) : TextureBaseColor（シャープ元画像）
// register(t1) : TextureBlurredColor（ブラー画像）- HLSL上の定義名を分ける
Texture2D<float4> TextureBlurredColor :register(t1);
// register(t2) : TexturePosition (G-BufferのWorldPosition)

PS_OUTPUT main(PS_IN input){
    PS_OUTPUT output;

    // 1. 各テクスチャからサンプリング
    float4 sharpColor = TextureBaseColor.Sample(Sampler, input.TexCoord);
    float4 blurredColor = TextureBlurredColor.Sample(Sampler, input.TexCoord);
    float4 worldPos = TexturePosition.Sample(Sampler, input.TexCoord);
    
    // 2. カメラとピクセル間のワールド空間距離の算出
    float distance = 0.0;

    if(dot(worldPos.xyz, worldPos.xyz) < 0.001){
        distance = 100000.0;
    }
    else{
        distance = length(worldPos.xyz - CameraPosition.xyz);
    }

    // 3. パラメータの取得
    float focusDist = Material.BaseColor.x;
    float focusRange = Material.BaseColor.y;
    float blurInt = Material.BaseColor.z;

    // 4. 錯乱円（Circle of Confusion - CoC）の計算
    // フォーカス範囲内はボケが0、範囲外になるにつれて1.0に近づく
    float coc = saturate(abs(distance - focusDist) / max(focusRange, 0.001));

    // 強度倍率の適用
    coc *= blurInt;

    // 5. シャープ画像とブラー画像の合成
    float3 finalColor = lerp(sharpColor.rgb, blurredColor.rgb, coc);

    output.Color = float4(finalColor, sharpColor.a);
    return output;

}