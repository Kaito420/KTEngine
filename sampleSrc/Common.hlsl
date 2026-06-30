


cbuffer EnvConstantBuffer : register(b0)
{
    float4		LightDirection;
    float4		LightColor;
    
    int         DiffuseModel; // 0:Lambert, 1:half-Lambert, 2:normalized-Lambert
    int         ShadingModel; // 0:Smooth, 1:Toon
    int         SpecularModel; // 0:off, 1:Phong
    int         RimLightModel; // 0:off, 1:on

    float RimPower; // リムライトの強さ
    float3 RimColor; // リムライトの色
};


cbuffer CameraConstantBuffer : register(b1)
{
    float4x4    View;
    float4x4    Projection;
	float4      CameraPosition;

};


cbuffer ObjectConstantBuffer : register(b2)
{
    float4x4    World;
};


cbuffer SubsetConstantBuffer : register(b3)
{
    struct MATERIAL
    {
        float4 BaseColor;
        float4 EmissionColor;
        float Metallic;
        float Specular;
        float Roughness;
        float NormalWeight;
        int ShadingModelID;
    } Material;
};






struct VS_INPUT
{
    float3 Position : POSITION;
    float3 Normal : NORMAL;
    float2 TexCoord : TEXCOORD;
    float4 Color : COLOR;
};


struct PS_INPUT
{
    float4 Position : SV_POSITION;
	float4 WorldPosition : POSITION;
    float4 Normal : NORMAL;
    float2 TexCoord : TEXCOORD;
    float4 Color : COLOR;
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

static float PI = 3.14159265359f;