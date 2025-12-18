//=====================================================================================
// VertexDirectionalLightingVS.hlsl
// Author:Kaito Aoki
// Date:2025/07/15
//=====================================================================================

#include "common.hlsl"

void main(in VS_IN In, out PS_IN Out)
{
    //頂点変換
    matrix wvp; //WorldViewProjection行列
    wvp = mul(World, View); //wvp = Wolrd * View
    wvp = mul(wvp, Projection); //wp = wvp * Projection
    
    Out.Position = mul(In.Position, wvp); //頂点座標を行列で変換
    
    //座標以外の要素を出力

    float4 worldNormal, normal; //ローカル変数を作成
    
    normal = float4(In.Normal.xyz, 0.0f);
    worldNormal = mul(normal, World);
    worldNormal = normalize(worldNormal);
    Out.Normal = worldNormal;
    
    float light = -dot(Light.Direction.xyz, Out.Normal.xyz);
    light = saturate(light);
    
    
    Out.Diffuse.rgb = light * In.Diffuse.rgb;
    Out.Diffuse.a = In.Diffuse.a;
    
    Out.TexCoord = In.TexCoord;

    
    

}