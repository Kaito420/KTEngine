//=====================================================================================
// UnlitTextureVS.hlsl
// Author:Kaito Aoki
// Date:2025/07/15
//=====================================================================================
#include "common.hlsl"

void main(in VS_IN In, out PS_IN Out)
{
    //頂点変換（必ず必要）
    matrix wvp; //WorldViewProjection行列
    wvp = mul(World, View); //wvp = Wolrd * View
    wvp = mul(wvp, Projection); //wp = wvp * Projection
    float4 position = float4(In.Position, 1.0f);
    Out.Position = mul(position, wvp);
    
    //座標以外の要素を出力
    float4 normal = float4(In.Normal.xyz, 0.0f);
    Out.Normal = normal;
    Out.TexCoord = In.TexCoord;
    Out.Diffuse = In.Diffuse * Material.Diffuse;


}