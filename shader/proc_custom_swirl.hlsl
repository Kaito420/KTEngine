// ============================================================================
// Procedural Texture HLSL (DirectX 12 Compute Shader: cs_5_0)
// - Available Inputs:
//   uint2  px     : Pixel coordinate (0 .. g_Width-1, 0 .. g_Height-1)
//   float2 uv     : Normalized UV (0..1) or warped UV from UV input pin
//   float3 pos3D  : 3D position on Sphere / Box (when 3D Mapping Space is active)
// - Available Uniforms (from Custom HLSL Node or Editor):
//   g_ParamA..D, g_ColorA, g_ColorB, g_ScaleX, g_ScaleY, g_Seed, g_MappingSpace
// - Available Input Samplers:
//   SampleInputA(uv), SampleInputB(uv), SampleInputUV(uv)
// ============================================================================

void EvaluateProceduralTexture(
	uint2 px,
	float2 uv,
	float3 pos3D,
	out float4 outAlbedo,
	out float4 outNormal,
	out float4 outARM)
{
	// Center coordinates (-1 .. +1)
	float2 centered = (uv - 0.5f) * 2.0f;
	float radius = length(centered);
	float angle  = atan2(centered.y, centered.x);

	// Swirl / Spiral distortion controlled by g_ParamA & g_ScaleX
	float swirlStrength = g_ParamA * 6.0f;
	float spiral = sin(angle * max(1.0f, round(g_ScaleX)) + radius * swirlStrength - Node_PerlinFBM(uv, pos3D, g_ScaleX, g_ScaleY, 4, 0.5f, 2.0f, g_Seed, false) * 4.0f);
	float mask = saturate(spiral * 0.5f + 0.5f);

	// Blend with optional In A pin if connected
	float4 inColA = SampleInputA(uv);
	float3 baseCol = lerp(g_ColorA.rgb, g_ColorB.rgb, mask);
	if (g_HasInA != 0) {
		baseCol = lerp(baseCol, inColA.rgb, saturate(g_ParamB));
	}

	outAlbedo = float4(baseCol, 1.0f);

	// Simple analytical normal from height mask
	outNormal = float4(0.5f, 0.5f, 1.0f, 1.0f);
	outARM    = float4(1.0f, lerp(0.25f, 0.85f, 1.0f - mask), 0.0f, 1.0f);
}
