#include "Common.hlsl"

PS_IN main(VS_IN input)
{
    PS_IN output;
    output.Position = float4(input.Position, 1.0f);
    output.WorldPosition = float4(input.Position, 1.0f);
    output.Normal = float4(input.Normal, 0.0f);
    output.TexCoord = input.TexCoord;
    output.Diffuse = input.Diffuse;
    
    return output;
}
