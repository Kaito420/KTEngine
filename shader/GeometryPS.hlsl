#include "Common.hlsl"

PS_OUTPUT_GEOMETRY main(PS_IN input)
{
    PS_OUTPUT_GEOMETRY output;
    
    // BaseColor
    if (Material.TextureEnable) {
        output.Color = TextureBaseColor.Sample(Sampler, input.TexCoord) * Material.BaseColor;
    } else {
        output.Color = Material.BaseColor;
    }
    
    // Normal
    if (Material.HasNormalMap) {
        float3 normalMap = TextureNormal.Sample(Sampler, input.TexCoord).rgb;
        normalMap = normalMap * 2.0f - 1.0f; // [0,1] → [-1,1]
        
        // ワールド空間法線からTBN行列を構築
        float3 N = normalize(input.Normal.xyz);
        // 法線に平行でない任意のベクトルを選択
        float3 up = abs(N.y) < 0.999f ? float3(0.0f, 1.0f, 0.0f) : float3(1.0f, 0.0f, 0.0f);
        float3 T = normalize(cross(up, N));
        float3 B = cross(N, T);
        
        // タンジェント空間 → ワールド空間
        float3 worldNormal = normalize(T * normalMap.x + B * normalMap.y + N * normalMap.z);
        
        // NormalWeight でブレンド
        worldNormal = normalize(lerp(N, worldNormal, Material.NormalWeight));
        
        output.Normal = float4(worldNormal, 1.0f);
    } else {
        output.Normal = input.Normal;
        output.Normal.a = 1.0f;
    }
    
    output.Position = input.WorldPosition;
    output.Position.a = 1.0f;

    // Material (ARM + ShadingModel)
    float3 arm = float3(1.0f, Material.Roughness, Material.Metallic);
    if (Material.HasMetallicMap) {
        // ARM Map is bound to register t2 (TexturePosition)
        arm = TexturePosition.Sample(Sampler, input.TexCoord).rgb;
    }
    
    output.MaterialARM.r = arm.r; // AO
    output.MaterialARM.g = arm.g; // Roughness
    output.MaterialARM.b = arm.b; // Metallic
    output.MaterialARM.a = (float)Material.ShadingModelID / 255.0f; // ShadingModelID
    
    return output;
}
