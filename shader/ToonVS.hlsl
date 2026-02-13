
#include "common.hlsl"

void main(in VS_IN In, out PS_IN Out)
{
    
    //頂点変換
    matrix wvp;
    wvp = mul(World, View);
    wvp = mul(wvp, Projection);
    float4 position = float4(In.Position, 1.0f);
    Out.Position = mul(position, wvp);
   
    //頂点の法線をワールド行列で変換
    float4 worldNormal = float4(In.Normal.xyz, 0.0f);
    worldNormal = mul(worldNormal, World);
    worldNormal = normalize(worldNormal);
    Out.Normal = worldNormal;
    
    Out.Diffuse = In.Diffuse;
    
    Out.TexCoord = In.TexCoord;
    
    Out.WorldPosition = mul(position, World);
}