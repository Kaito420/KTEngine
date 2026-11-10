#include "Common.hlsl"
#include "ProceduralCommon.hlsl"

PS_OUTPUT_GEOMETRY main(PS_IN input, bool isFrontFace : SV_IsFrontFace)
{
    PS_OUTPUT_GEOMETRY output;

    int mappingMode = Material.HasRoughnessMap;
    int wrapMode    = GetMaterialWrapMode();
    int uvScaleMode = GetMaterialUVScaleMode();
    float scale3D   = GetMaterialScale3D();
    int seed        = GetMaterialSeed();
    float2 tiling   = GetMaterialTiling();
    float2 offset   = GetMaterialOffset();

    float3 localPos    = input.Diffuse.xyz;
    float3 localNormal = normalize(float3(input.WorldPosition.w, input.Normal.w, input.Diffuse.w));
    if (length(localNormal) < 0.001f)
    {
        localNormal = float3(0.0f, 1.0f, 0.0f);
    }

    float3 baseNormal = normalize(input.Normal.xyz) * (isFrontFace ? 1.0f : -1.0f);

    // =========================================================================
    // Mode 3: Real-Time 3D Procedural HLSL Evaluation in Pixel Shader
    // Default Built-in Sample: 3D Voxel Checker/Grid x 3D Perlin FBM Hybrid
    // (Evaluated directly from 3D coordinates without any 2D texture lookups)
    // =========================================================================
    if (mappingMode == 3)
    {
        float3 objScale = (uvScaleMode == 0) ? GetObjectScaleFromWorld() : float3(1.0f, 1.0f, 1.0f);
        float3 procPos3D = localPos * 2.0f * objScale * scale3D + float3(offset.x, 0.0f, offset.y);

        float4 procAlbedo;
        float3 procNormalOffset;
        float3 procARM;
        EvaluateDefaultProcedural3D(procPos3D, seed, procAlbedo, procNormalOffset, procARM);

        output.Color = float4(procAlbedo.rgb * Material.BaseColor.rgb + Material.EmissionColor.rgb, Material.BaseColor.a);

        float3 worldOffset = mul(float4(procNormalOffset, 0.0f), World).xyz;
        float3 worldNormal = normalize(baseNormal + worldOffset * Material.NormalWeight);
        output.Normal = float4(worldNormal, 1.0f);

        output.Position = float4(input.WorldPosition.xyz, 1.0f);
        output.MaterialARM.r = procARM.r;
        output.MaterialARM.g = procARM.g;
        output.MaterialARM.b = procARM.b;
        output.MaterialARM.a = (float)Material.ShadingModelID / 255.0f;
        return output;
    }

    // =========================================================================
    // Mode 0 (2D UV), Mode 1 (3D Solid PNG), Mode 2 (3D World Triplanar PNG)
    // =========================================================================
    float2 sampleUV = input.TexCoord;
    bool useSphericalSolid = (mappingMode == 1);
    bool useTriplanar      = (mappingMode == 2);

    float3 triplanarPos3D = localPos;
    if (useSphericalSolid)
    {
        sampleUV = Dir3DToSphericalUV(localPos);
        if (Material.FlipU != 0) sampleUV.x = 1.0f - sampleUV.x;
        if (Material.FlipV != 0) sampleUV.y = 1.0f - sampleUV.y;
        sampleUV = sampleUV * tiling + offset;
    }
    else if (useTriplanar)
    {
        float3 objScale = (uvScaleMode == 0) ? GetObjectScaleFromWorld() : float3(1.0f, 1.0f, 1.0f);
        triplanarPos3D = localPos * objScale * scale3D;
    }

    // 1. BaseColor
    if (Material.TextureEnable)
    {
        float4 texCol;
        if (useSphericalSolid)
        {
            texCol = TextureBaseColor.SampleLevel(Sampler, frac(sampleUV), 0);
        }
        else if (useTriplanar)
        {
            texCol = SampleTexture3DTriplanar(TextureBaseColor, triplanarPos3D, localNormal, tiling, offset, wrapMode, seed);
        }
        else
        {
            texCol = SampleTexture2DWithWrapMode(TextureBaseColor, sampleUV, wrapMode, seed);
        }
        output.Color = texCol * Material.BaseColor;
    }
    else
    {
        output.Color = Material.BaseColor;
    }
    output.Color.rgb += Material.EmissionColor.rgb;

    // 2. Normal
    if (Material.HasNormalMap)
    {
        float3 normalMap;
        if (useSphericalSolid)
        {
            normalMap = TextureNormal.SampleLevel(Sampler, frac(sampleUV), 0).rgb;
        }
        else if (useTriplanar)
        {
            normalMap = SampleTexture3DTriplanar(TextureNormal, triplanarPos3D, localNormal, tiling, offset, wrapMode, seed).rgb;
        }
        else
        {
            normalMap = SampleTexture2DWithWrapMode(TextureNormal, sampleUV, wrapMode, seed).rgb;
        }
        normalMap = normalMap * 2.0f - 1.0f;

        float3 N = baseNormal;
        float3 up = abs(N.y) < 0.999f ? float3(0.0f, 1.0f, 0.0f) : float3(1.0f, 0.0f, 0.0f);
        float3 T = normalize(cross(up, N));
        float3 B = cross(N, T);

        float3 worldNormal = normalize(T * normalMap.x + B * normalMap.y + N * normalMap.z);
        worldNormal = normalize(lerp(N, worldNormal, Material.NormalWeight));
        output.Normal = float4(worldNormal, 1.0f);
    }
    else
    {
        output.Normal = float4(baseNormal, 1.0f);
    }

    // 3. World Position
    output.Position = float4(input.WorldPosition.xyz, 1.0f);

    // 4. Material (ARM + ShadingModel)
    float3 arm = float3(1.0f, Material.Roughness, Material.Metallic);
    if (Material.HasMetallicMap)
    {
        if (useSphericalSolid)
        {
            arm = TexturePosition.SampleLevel(Sampler, frac(sampleUV), 0).rgb;
        }
        else if (useTriplanar)
        {
            arm = SampleTexture3DTriplanar(TexturePosition, triplanarPos3D, localNormal, tiling, offset, wrapMode, seed).rgb;
        }
        else
        {
            arm = SampleTexture2DWithWrapMode(TexturePosition, sampleUV, wrapMode, seed).rgb;
        }
    }

    output.MaterialARM.r = arm.r;
    output.MaterialARM.g = arm.g;
    output.MaterialARM.b = arm.b;
    output.MaterialARM.a = (float)Material.ShadingModelID / 255.0f;

    return output;
}
