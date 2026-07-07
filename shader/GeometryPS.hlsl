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
        output.Normal = float4(normalize(normalMap), 1.0f);
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
