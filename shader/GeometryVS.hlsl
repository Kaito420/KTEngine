#include "Common.hlsl"

PS_IN main(VS_IN input)
{
    PS_IN output;
    
    float4x4 wvp;
    wvp = mul(World, View);
    wvp = mul(wvp, Projection);
    
    float4 position = float4(input.Position, 1.0f);
    output.Position = mul(position, wvp);
    output.WorldPosition = mul(position, World);
    
    float4 normal = float4(input.Normal, 0.0f);
    output.Normal = mul(normal, World);
    
    output.TexCoord = input.TexCoord;
    if (Material.FlipU != 0)
    {
        output.TexCoord.x = 1.0f - output.TexCoord.x;
    }
    if (Material.FlipV != 0)
    {
        output.TexCoord.y = 1.0f - output.TexCoord.y;
    }
    
    output.Diffuse = input.Diffuse;
    
    return output;
}
