// ============================================================================
// Auto-Generated HLSL from Procedural Texture Node Graph
// Compatible with DirectX 12 Compute Shader (cs_5_0) & Custom HLSL Node
// ============================================================================

void EvaluateNodeGraphAt(float2 uv, float3 pos3D, out float4 outAlbedo, out float4 outARM, out float outHeightForNormal, out float outNormalStrength, out float outNormalSignY, out bool outHasNormalNode)
{
	outHasNormalNode = false;
	outHeightForNormal = 0.5f;
	outNormalStrength = 3.0f;
	outNormalSignY = 1.0f;

	// --- Node #2: Perlin / FBM Noise ---
	float v2 = Node_PerlinFBM(uv, pos3D, 3.0000f, 2.0000f, 3, 0.5000f, 2.0000f, 101, false);
	float4 n2 = float4(v2, v2, v2, 1.0f);

	// --- Node #3: UV Warp ---
	float4 n3 = n2;

	// --- Node #4: Perlin / FBM Noise ---
	float v4 = Node_PerlinFBM(n3.xy, MapUVTo3D(n3.xy, g_MappingSpace), 14.0000f, 2.0000f, 4, 0.4500f, 2.0000f, 202, true);
	float4 n4 = float4(v4, v4, v4, 1.0f);

	// --- Node #5: Perlin / FBM Noise ---
	float v5 = Node_PerlinFBM(uv, pos3D, 32.0000f, 3.0000f, 5, 0.6000f, 2.0000f, 303, false);
	float4 n5 = float4(v5, v5, v5, 1.0f);

	// --- Node #6: Blend ---
	float4 n6 = Node_Blend(n4, n5, 0.3000f, 1.0f, 1);

	// --- Node #7: Color Ramp ---
	float4 n7 = Node_ColorRamp(Luminance(n6.rgb), true, 0.0000f, 0.5500f, 0.9000f, float3(0.1000f, 0.0800f, 0.0700f), float3(0.5500f, 0.3500f, 0.2000f), float3(0.9200f, 0.8500f, 0.7200f));

	// --- Node #8: Height to Normal ---
	outHasNormalNode = true;
	outHeightForNormal = Luminance(n6.rgb);
	outNormalStrength = 2.4000f;
	outNormalSignY = 1.0f;
	float4 n8 = float4(0.5f, 0.5f, 1.0f, 1.0f);

	// --- Node #9: Levels / Math ---
	float4 n9 = Node_Levels(n6, 0.1600f, 2.0400f, 1.0100f, true);

	// --- Composite Output ---
	outAlbedo = n7;
	float ao    = 1.000000f;
	float rough = Luminance(n9.rgb);
	float metal = 0.000000f;
	outARM = float4(ao, rough, metal, 1.0f);
}

void EvaluateProceduralTexture(
	uint2 px,
	float2 uv,
	float3 pos3D,
	out float4 outAlbedo,
	out float4 outNormal,
	out float4 outARM)
{
	float hCenter, normStrength, normSignY;
	bool hasNormal;
	EvaluateNodeGraphAt(uv, pos3D, outAlbedo, outARM, hCenter, normStrength, normSignY, hasNormal);
	outNormal = float4(0.5f, 0.5f, 1.0f, 1.0f);
	if (hasNormal) {
		float2 texel = 1.0f / float2(max(1u, g_Width), max(1u, g_Height));
		float4 dummyA, dummyARM;
		float hL, hR, hU, hD, dummyS, dummyY;
		bool dummyHas;
		float2 uvL = frac(uv + float2(-texel.x, 0.0f));
		float2 uvR = frac(uv + float2( texel.x, 0.0f));
		float2 uvU = frac(uv + float2(0.0f, -texel.y));
		float2 uvD = frac(uv + float2(0.0f,  texel.y));
		EvaluateNodeGraphAt(uvL, MapUVTo3D(uvL, g_MappingSpace), dummyA, dummyARM, hL, dummyS, dummyY, dummyHas);
		EvaluateNodeGraphAt(uvR, MapUVTo3D(uvR, g_MappingSpace), dummyA, dummyARM, hR, dummyS, dummyY, dummyHas);
		EvaluateNodeGraphAt(uvU, MapUVTo3D(uvU, g_MappingSpace), dummyA, dummyARM, hU, dummyS, dummyY, dummyHas);
		EvaluateNodeGraphAt(uvD, MapUVTo3D(uvD, g_MappingSpace), dummyA, dummyARM, hD, dummyS, dummyY, dummyHas);
		float dX = (hR - hL) * 4.0f * normStrength;
		float dY = (hD - hU) * 4.0f * normStrength * normSignY;
		float3 n = normalize(float3(-dX, -dY, 1.0f));
		outNormal = float4(saturate(n * 0.5f + 0.5f), 1.0f);
	}
}
