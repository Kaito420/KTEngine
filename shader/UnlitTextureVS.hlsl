//=====================================================================================
// UnlitTextureVS.hlsl
// Author:Kaito Aoki
// Date:2025/07/15
//=====================================================================================
#include "Common.hlsl"

void main(in VS_IN In, out PS_IN Out)
{
    matrix wvp;
    wvp = mul(World, View);
    wvp = mul(wvp, Projection);
    float4 position = float4(In.Position, 1.0f);
    Out.Position = mul(position, wvp);
    
    float4 normal = float4(In.Normal.xyz, 0.0f);
    Out.Normal = normal;
    Out.TexCoord = In.TexCoord;
    if (Material.FlipU != 0)
    {
        Out.TexCoord.x = 1.0f - Out.TexCoord.x;
    }
    if (Material.FlipV != 0)
    {
        Out.TexCoord.y = 1.0f - Out.TexCoord.y;
    }
    Out.Diffuse = In.Diffuse * Material.Diffuse;
}