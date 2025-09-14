//=====================================================================================
// UnlitTexturePS.hlsl
// Author:Kaito Aoki
// Date:2025/07/15
//=====================================================================================

#include "common.hlsl"  //必ずインクルード
//DirectXのテクスチャ設定を受け継ぐ
Texture2D g_Texture : register(t0); //テクスチャ0番
//DirectXのサンプラーステート設定を受け継ぐ
SamplerState g_SamplerState : register(s0); //テクスチャサンプラー0番

void main(in PS_IN In, out float4 outDiffuse : SV_Target)
{
    if (Material.TextureEnable == 1)
    {
        //このピクセルに使われるテクセル（テクスチャの色）を取得
        outDiffuse = g_Texture.Sample(g_SamplerState, In.TexCoord);
        outDiffuse *= In.Diffuse; //マテリアルのデフューズ色を乗算
        //outDiffuse = float4(0, 0, 0, 1);

    }
    else
    {
        //さらにピクセルのデフューズ色を乗算しておく
        //（頂点色にテクスチャ色が影響される）
        outDiffuse = In.Diffuse;
    }

}