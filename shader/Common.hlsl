//=====================================================================================
// Common.hlsl
// Author:Kaito Aoki
// Date:2025/07/15
//=====================================================================================

cbuffer WorldBuffer : register(b0){
    matrix World;
}

cbuffer ViewBuffer : register(b1){
    matrix View;
}

cbuffer ProjectionBuffer : register(b2){
    matrix Projection;
}

struct MATERIAL
{
    float4 Ambient;
    float4 Diffuse;
    float4 Specular;
    float4 Emission;
    float Shininess;
    bool TextureEnable;
    float2 Dummy;
};

cbuffer MaterialBuffer : register(b3)
{
    MATERIAL Material;
}

struct VS_IN{
    float4 Position : POSITION0;
    float4 Normal : NORMAL0;
    float4 Diffuse : COLOR0;
    float2 TexCoord : TEXCOORD0;
};

struct PS_IN{
    float4 Position : SV_POSITION;
    float4 WorldPosition : POSITION0;
    float4 Normal : NORMAL0;
    float4 Diffuse : COLOR0;
    float2 TexCoord : TEXCOORD0;
};