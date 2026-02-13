//=====================================================================================
// UnlitColorVS.hlsl
// Author:Kaito Aoki
// Date:2025/07/15
//=====================================================================================

#include "Common.hlsl"

void main(in VS_IN In, out PS_IN Out)
{
    //í∏ì_ïœä∑
    matrix wvp;     //WorldViewProjectionçsóÒ
    wvp = mul(World, View);
    wvp = mul(wvp, Projection);
    float4 position = float4(In.Position, 1.0f);
    Out.Position = mul(position, wvp);
    float4 normal = float4(In.Normal.xyz, 0.0f);
    Out.Normal = normal;
    Out.TexCoord = In.TexCoord;
    Out.Diffuse = In.Diffuse;

}