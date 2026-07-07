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
    // [新しいマテリアルメンバ] (PBR / G-Buffer 用)
    float4 BaseColor;
    float4 EmissionColor;
    float Metallic;
    float Specular;
    float Roughness;
    float NormalWeight;
    int ShadingModelID;
    int FlipU;
    int FlipV;
    int HasNormalMap;
    
    // [古いマテリアルメンバ] (既存フォワード用)
    float4 Ambient;
    float4 Diffuse;
    float4 SpecularOld;
    float4 Emission;
    float Shininess;
    bool TextureEnable;
    int HasMetallicMap;
    int HasRoughnessMap;
};

cbuffer MaterialBuffer : register(b3)
{
    MATERIAL Material;
}

struct LIGHT
{
    bool Enable;
    int DiffuseModel;   // 0:Lambert, 1:half-Lambert, 2:normalized-Lambert
    int ShadingModel;   // 0:Smooth, 1:Toon
    int SpecularModel;  // 0:off, 1:Phong
    
    float4 Direction;
    float4 Diffuse;
    float4 Ambient;
    float4 Position;
    float4 Parameter;   // 予備パラメータ
    
    float4 RimColor;
    float RimPower;
    int RimLightModel;
    float Intensity;
    float AmbientIntensity;
    
    float Exposure;
    float3 DummyLight;
};

cbuffer LightBuffer : register(b4)
{
    LIGHT Light;
}

cbuffer CameraBuffer : register(b5)
{
    float4 CameraPosition;
}

struct VS_IN{
    float3 Position : POSITION0;
    float3 Normal : NORMAL0;
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

struct PS_OUTPUT
{
    float4 Color : SV_TARGET0;
};

struct PS_OUTPUT_GEOMETRY
{
    float4 Color : SV_TARGET0;
    float4 Normal : SV_TARGET1;
    float4 Position : SV_TARGET2;
    float4 MaterialMetallic : SV_TARGET3;
    float4 MaterialSpecular : SV_TARGET4;
    float4 MaterialRoughness : SV_TARGET5;
};

Texture2D<float4> TextureBaseColor : register(t0);
Texture2D<float4> TextureNormal : register(t1);
Texture2D<float4> TexturePosition : register(t2);
Texture2D<float4> TextureMaterialMetallic : register(t3);
Texture2D<float4> TextureMaterialSpecular : register(t4);
Texture2D<float4> TextureMaterialRoughness : register(t5);

SamplerState Sampler : register(s0);