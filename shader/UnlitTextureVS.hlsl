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
    
    Out.Position = mul(In.Position, wvp); //頂点座標を行列で変換
    
    //座標以外の要素を出力
    Out.Normal = In.Normal;
    Out.TexCoord = In.TexCoord;
    Out.Diffuse = In.Diffuse;


}