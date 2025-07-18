//=====================================================================================
// UnlitColorPS.hlsl
// Author:Kaito Aoki
// Date:2025/07/15
//=====================================================================================

#include "Common.hlsl"

void main(in PS_IN In, out float4 outDiffuse :SV_Target)
{
    outDiffuse = In.Diffuse;
}