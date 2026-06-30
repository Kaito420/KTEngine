#include "Common.hlsl"

PS_OUTPUT main(PS_INPUT input)
{
    PS_OUTPUT output;
    
    output.Color = TextureBaseColor.Sample(Sampler, input.TexCoord);
    
    float3 normal = normalize(input.Normal); // 法線
    float3 lightDir = normalize(LightDirection.xyz); //光源位置
    
    float diffuse = dot(normal, lightDir); // ランバート拡散照明
    diffuse = saturate(diffuse);
    
    output.Color.rgb *= diffuse * LightColor; // 拡散照明を色に適用
    
    return output;
}