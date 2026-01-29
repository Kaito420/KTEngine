
#include "common.hlsl"

Texture2D g_Texture : register(t0);
SamplerState g_SamplerState : register(s0);

void main(in PS_IN In, out float4 outDiffuse : SV_Target)
{
    //光源からピクセルへのベクトル
    float4 lv = In.WorldPosition - Light.Position;
    float4 ld = length(lv); //物体と光源の距離
    lv = normalize(lv); //正規化
    
    //減衰の計算
    float ofs = 1.0f - (1.0f / Light.Parameter.x) * ld;
    //減衰率0未満は0にする
    ofs = max(0, ofs);
    
    //ピクセルの法線を正規化
    float4 normal = normalize(In.Normal);
    float light = -dot(normal.xyz, lv.xyz);
    light = saturate(light);
    light *= ofs;
    
    //トゥーン処理
    if (light > 0.7f)
    {
        light = 1.0f;
    }
    else if (light > 0.4f)
    {
        light = 0.7f;
    }
    else if (light > 0.2f)
    {
        light = 0.4f;
    }
    else
    {
        light = 0.2f;
    }
    
    
    //テクスチャのピクセル色を取得
    outDiffuse = g_Texture.Sample(g_SamplerState, In.TexCoord);
    outDiffuse.rgb *= In.Diffuse.rgb * light + Light.Ambient.rgb;
    outDiffuse.a *= In.Diffuse.a;
    
    
    //カメラからピクセルへ向かうベクトル
    float3 eyev = In.WorldPosition.xyz - CameraPosition.xyz;
    eyev = normalize(eyev);
    
    //エッジ処理
        //光の方向と視線ベクトルの考慮（逆光程明るい）
    float lit;
    lit = dot(lv.xyz, eyev);
    lit = 1 - max(0, lit);
    
    //輪郭部分ほど明るくする
    float rim;
    rim = dot(eyev, normal.xyz);
    rim = 1 - max(0, -rim);
    rim *= lit;
    
    rim = pow(rim, 3);
    
    //リムライティングの明るさをデフューズに加算して出力
    outDiffuse.rgb += rim;
    
}