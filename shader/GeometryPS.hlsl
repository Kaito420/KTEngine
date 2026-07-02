#include "Common.hlsl"

PS_OUTPUT_GEOMETRY main(PS_IN input)
{
    PS_OUTPUT_GEOMETRY output;
    
    output.Color = TextureBaseColor.Sample(Sampler, input.TexCoord) * Material.BaseColor;
    
    output.Normal = input.Normal;
    output.Normal.a = 1.0f;
    
    output.Position = input.WorldPosition;
    output.Position.a = 1.0f;

    output.MaterialMetallic = float4(Material.Metallic, Material.Metallic, Material.Metallic, 1.0f);
    output.MaterialSpecular = float4(Material.Specular, Material.Specular, Material.Specular, 1.0f);
    output.MaterialRoughness = float4(Material.Roughness, Material.Roughness, Material.Roughness, 1.0f);
    return output;
}
