//=====================================================================================
// ProcWoodPS.hlsl
// Real-Time 3D Solid Wood Procedural Pixel Shader (G-Buffer Compatible, ps_5_0)
// Evaluates 3D Perlin FBM Noise & 3D Cylindrical Annual Rings directly per pixel
//=====================================================================================

#include "Common.hlsl"
#include "ProceduralCommon.hlsl"

void EvaluateProceduralTexture(
    uint2 px,
    float2 uv,
    float3 pos3D,
    out float4 outAlbedo,
    out float4 outNormal,
    out float4 outARM)
{
    int seed = GetMaterialSeed();

    // 1. 3D Low-frequency Perlin FBM distortion for natural wood grain waviness
    float warpNoise = Node_PerlinFBM(uv, pos3D, 3.0f, 1.5f, 4, 0.5f, 2.0f, seed, false);
    float3 warpedPos = pos3D + (warpNoise - 0.5f) * float3(0.20f, 0.06f, 0.20f);

    // 2. 3D Cylindrical annual rings with off-center log axis (produces planks / cathedrals)
    float4 ringGrad = Node_Gradient(
        uv,
        warpedPos,
        1,     // Radial (3D Cylindrical rings)
        40.0f, // Axis Offset angle
        9.0f,  // Ring Repeat count
        float3(0.0f, 0.0f, 0.0f),
        float3(1.0f, 1.0f, 1.0f)
    );

    // 3. High-frequency vertical 3D wood fibers along Y
    float fiberNoise = Node_PerlinFBM(uv, pos3D * float3(10.0f, 1.5f, 10.0f), 8.0f, 2.0f, 3, 0.5f, 2.0f, seed + 503, false);
    float woodFactor = saturate(ringGrad.r * 0.84f + (fiberNoise - 0.5f) * 0.20f);

    // 4. Map wood factor to 3-stop Wood Color Ramp
    outAlbedo = Node_ColorRamp(
        woodFactor,
        true,
        0.08f, 0.48f, 0.94f,
        float3(0.24f, 0.12f, 0.05f), // Dark latewood ring
        float3(0.56f, 0.33f, 0.16f), // Mid heartwood
        float3(0.76f, 0.54f, 0.30f)  // Light earlywood
    );

    // 5. Tangent-space normal & ARM output
    float dX = ddx(woodFactor) * 4.0f;
    float dY = ddy(woodFactor) * 4.0f;
    float3 n = normalize(float3(-dX, -dY, 1.0f));
    outNormal = float4(saturate(n * 0.5f + 0.5f), 1.0f);
    outARM    = float4(1.0f, lerp(0.38f, 0.72f, 1.0f - woodFactor), 0.0f, 1.0f);
}

PS_OUTPUT_GEOMETRY main(PS_IN input, bool isFrontFace : SV_IsFrontFace)
{
    PS_OUTPUT_GEOMETRY output;

    int uvScaleMode = GetMaterialUVScaleMode();
    float scale3D   = GetMaterialScale3D();
    float2 offset   = GetMaterialOffset();

    float3 localPos  = input.Diffuse.xyz;
    float3 objScale  = (uvScaleMode == 0) ? GetObjectScaleFromWorld() : float3(1.0f, 1.0f, 1.0f);
    float3 procPos3D = localPos * 2.0f * objScale * scale3D + float3(offset.x, 0.0f, offset.y);

    float4 procAlbedo = float4(0.5f, 0.5f, 0.5f, 1.0f);
    float4 procNormal = float4(0.5f, 0.5f, 1.0f, 1.0f);
    float4 procARM    = float4(1.0f, Material.Roughness, Material.Metallic, 1.0f);

    EvaluateProceduralTexture((uint2)input.Position.xy, input.TexCoord, procPos3D, procAlbedo, procNormal, procARM);

    output.Color = float4(procAlbedo.rgb * Material.BaseColor.rgb + Material.EmissionColor.rgb, Material.BaseColor.a);

    float3 baseNormal = normalize(input.Normal.xyz) * (isFrontFace ? 1.0f : -1.0f);
    float3 normalMap  = procNormal.rgb * 2.0f - 1.0f;
    float3 N  = baseNormal;
    float3 up = abs(N.y) < 0.999f ? float3(0.0f, 1.0f, 0.0f) : float3(1.0f, 0.0f, 0.0f);
    float3 T  = normalize(cross(up, N));
    float3 B  = cross(N, T);
    float3 worldNormal = normalize(T * normalMap.x + B * normalMap.y + N * normalMap.z);
    worldNormal = normalize(lerp(N, worldNormal, Material.NormalWeight));
    output.Normal = float4(worldNormal, 1.0f);

    output.Position = float4(input.WorldPosition.xyz, 1.0f);

    output.MaterialARM.r = procARM.r;
    output.MaterialARM.g = saturate(procARM.g * (Material.Roughness * 2.0f));
    output.MaterialARM.b = max(procARM.b, Material.Metallic);
    output.MaterialARM.a = (float)Material.ShadingModelID / 255.0f;

    return output;
}
