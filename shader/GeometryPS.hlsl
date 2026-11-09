#include "Common.hlsl"

PS_OUTPUT_GEOMETRY main(PS_IN input, bool isFrontFace : SV_IsFrontFace)
{
    PS_OUTPUT_GEOMETRY output;
    
    float2 sampleUV = input.TexCoord;
    bool useSolidMapping = (Material.HasRoughnessMap != 0);
    if (useSolidMapping) {
        float3 lp = input.Diffuse.xyz;
        float len = length(lp);
        float3 n = (len > 0.0001f) ? (lp / len) : float3(0.0f, 1.0f, 0.0f);
        float phi = atan2(n.z, n.x);
        if (phi < 0.0f) phi += 6.2831853f;
        sampleUV.x = frac(phi / 6.2831853f);
        sampleUV.y = saturate(acos(clamp(n.y, -1.0f, 1.0f)) / 3.14159265f);
        if (Material.FlipU != 0) sampleUV.x = 1.0f - sampleUV.x;
        if (Material.FlipV != 0) sampleUV.y = 1.0f - sampleUV.y;
    }

    // BaseColor
    if (Material.TextureEnable) {
        float4 texCol = useSolidMapping
            ? TextureBaseColor.SampleLevel(Sampler, sampleUV, 0)
            : TextureBaseColor.Sample(Sampler, sampleUV);
        output.Color = texCol * Material.BaseColor;
    } else {
        output.Color = Material.BaseColor;
    }
    output.Color.rgb += Material.EmissionColor.rgb;
    float3 baseNormal = normalize(input.Normal.xyz) * (isFrontFace ? 1.0f : -1.0f);
    if (false) {
    }
    
    // Normal
    if (Material.HasNormalMap) {
        float3 normalMap = (useSolidMapping
            ? TextureNormal.SampleLevel(Sampler, sampleUV, 0)
            : TextureNormal.Sample(Sampler, sampleUV)).rgb;
        normalMap = normalMap * 2.0f - 1.0f; // [0,1] → [-1,1]
        
        // ワールド空間法線からTBN行列を構築
        float3 N = baseNormal;
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
        output.Normal = float4(baseNormal, 1.0f);
        output.Normal.a = 1.0f;
    }
    
    output.Position = input.WorldPosition;
    output.Position.a = 1.0f;

    // Material (ARM + ShadingModel)
    float3 arm = float3(1.0f, Material.Roughness, Material.Metallic);
    if (Material.HasMetallicMap) {
        // ARM Map is bound to register t2 (TexturePosition)
        arm = (useSolidMapping
            ? TexturePosition.SampleLevel(Sampler, sampleUV, 0)
            : TexturePosition.Sample(Sampler, sampleUV)).rgb;
    }
    
    output.MaterialARM.r = arm.r; // AO
    output.MaterialARM.g = arm.g; // Roughness
    output.MaterialARM.b = arm.b; // Metallic
    output.MaterialARM.a = (float)Material.ShadingModelID / 255.0f; // ShadingModelID
    
    return output;
}
