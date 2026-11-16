#include "Common.hlsl"

PS_IN main(VS_IN input)
{
    PS_IN output;
    
    float4x4 wvp;
    wvp = mul(World, View);
    wvp = mul(wvp, Projection);
    
    float4 position = float4(input.Position, 1.0f);
    float4 normal = float4(input.Normal, 0.0f);
    ApplySkinning(position, normal, input.BoneIndices, input.BoneWeights);

    output.Position = mul(position, wvp);
    
    float3 localN = normalize(normal.xyz);
    // Transform normal by Inverse-Transpose of World's 3x3 basis so non-uniform Scale preserves exact slope normals
    float3 axisX = mul(float4(1.0f, 0.0f, 0.0f, 0.0f), World).xyz;
    float3 axisY = mul(float4(0.0f, 1.0f, 0.0f, 0.0f), World).xyz;
    float3 axisZ = mul(float4(0.0f, 0.0f, 1.0f, 0.0f), World).xyz;
    float invSqX = 1.0f / max(1e-8f, dot(axisX, axisX));
    float invSqY = 1.0f / max(1e-8f, dot(axisY, axisY));
    float invSqZ = 1.0f / max(1e-8f, dot(axisZ, axisZ));
    float3 worldNormal = normalize(localN.x * (axisX * invSqX) + localN.y * (axisY * invSqY) + localN.z * (axisZ * invSqZ));

    output.WorldPosition = float4(mul(position, World).xyz, localN.x);
    output.Normal        = float4(worldNormal,              localN.y);
    output.Diffuse       = float4(position.xyz,             localN.z);
    
    float2 uv = input.TexCoord;
    if (Material.FlipU != 0)
    {
        uv.x = 1.0f - uv.x;
    }
    if (Material.FlipV != 0)
    {
        uv.y = 1.0f - uv.y;
    }

    // UE5-style 2D Auto-Loop by Object Scale (when MappingMode == 0 and UVScaleMode == 1)
    if (Material.HasRoughnessMap == 0 && Material.SpecularOld.y > 0.5f)
    {
        float3 objScale = float3(
            length(mul(float4(1.0f, 0.0f, 0.0f, 0.0f), World).xyz),
            length(mul(float4(0.0f, 1.0f, 0.0f, 0.0f), World).xyz),
            length(mul(float4(0.0f, 0.0f, 1.0f, 0.0f), World).xyz)
        );
        float3 an = abs(localN);
        float scaleU = (an.x > 0.5f) ? objScale.z : objScale.x;
        float scaleV = (an.y > 0.5f) ? objScale.z : objScale.y;
        uv.x *= max(0.001f, scaleU);
        uv.y *= max(0.001f, scaleV);
    }

    // Apply Material UV Tiling (Ambient.xy) and UV Offset (Ambient.zw)
    float2 tiling = Material.Ambient.xy;
    if (abs(tiling.x) < 0.0001f && abs(tiling.y) < 0.0001f)
    {
        tiling = float2(1.0f, 1.0f);
    }
    output.TexCoord = uv * tiling + Material.Ambient.zw;
    
    return output;
}
