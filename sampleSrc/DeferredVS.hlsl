#include "Common.hlsl"

PS_INPUT main(VS_INPUT input)
{
    PS_INPUT output;
    output.Position = float4(input.Position, 1.0f);
    output.Normal = float4(input.Normal, 0.0f);
    output.TexCoord = input.TexCoord;
    output.Color = input.Color;
    
    return output;
}