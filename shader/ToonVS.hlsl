
#include "common.hlsl"

void main(in VS_IN In, out PS_IN Out)
{
    
    //頂点変換
    matrix wvp;
    wvp = mul(World, View);
    wvp = mul(wvp, Projection);
    
    Out.Position = mul(In.Position, wvp);
   
    //頂点の法線をワールド行列で変換
    float4 worldNormal = float4(In.Normal.xyz, 0.0f);
    worldNormal = mul(worldNormal, World);
    worldNormal = normalize(worldNormal);
    Out.Normal = worldNormal;
    
    Out.Diffuse = In.Diffuse;
    
    Out.TexCoord = In.TexCoord;
    
    Out.WorldPosition = mul(In.Position, World);
}