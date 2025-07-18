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
    Out.Position = mul(In.Position, wvp);
    
    Out.Normal = In.Normal;
    Out.TexCoord = In.TexCoord;
    Out.Diffuse = In.Difuuse;

}