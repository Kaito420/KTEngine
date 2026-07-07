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

    // Metallic
    if (Material.HasMetallicMap) {
        float metallic = TexturePosition.Sample(Sampler, input.TexCoord).r; // t2
        output.MaterialMetallic = float4(metallic, metallic, metallic, 1.0f);
    } else {
        output.MaterialMetallic = float4(Material.Metallic, Material.Metallic, Material.Metallic, 1.0f);
    }
    
    // Specular (スカラー値のみ)
    output.MaterialSpecular = float4(Material.Specular, Material.Specular, Material.Specular, 1.0f);
    
    // Roughness
    if (Material.HasRoughnessMap) {
        float roughness = TextureMaterialMetallic.Sample(Sampler, input.TexCoord).r; // t3
        output.MaterialRoughness = float4(roughness, roughness, roughness, 1.0f);
    } else {
        output.MaterialRoughness = float4(Material.Roughness, Material.Roughness, Material.Roughness, 1.0f);
    }
    
    return output;
}
