//=====================================================================================
// ProceduralCommon.hlsl
// 3D Procedural Noise Library & UE5-Style UV / Triplanar / Solid Mapping Helpers
// Compatible with DirectX 12 Pixel Shaders (ps_5_0)
//=====================================================================================

#ifndef _PROCEDURAL_COMMON_HLSL
#define _PROCEDURAL_COMMON_HLSL

static const float PROC_PI = 3.14159265f;

// Uniform defaults used by Node_* functions inside Pixel Shaders
static const int  g_MappingSpace = 2; // 3D Solid / Box-Sphere space in Pixel Shader
static const uint g_Width        = 512u;
static const uint g_Height       = 512u;

float Luminance(float3 c)
{
    return dot(c, float3(0.299f, 0.587f, 0.114f));
}

// Extract object scale (Sx, Sy, Sz) from World matrix (register b0)
float3 GetObjectScaleFromWorld()
{
    float sx = length(mul(float4(1.0f, 0.0f, 0.0f, 0.0f), World).xyz);
    float sy = length(mul(float4(0.0f, 1.0f, 0.0f, 0.0f), World).xyz);
    float sz = length(mul(float4(0.0f, 0.0f, 1.0f, 0.0f), World).xyz);
    return max(float3(sx, sy, sz), float3(0.001f, 0.001f, 0.001f));
}

float2 GetMaterialTiling()
{
    float2 t = Material.Ambient.xy;
    bool isZero = (abs(t.x) < 0.0001f && abs(t.y) < 0.0001f);
    return isZero ? float2(1.0f, 1.0f) : t;
}

float2 GetMaterialOffset()
{
    return Material.Ambient.zw;
}

int GetMaterialWrapMode()
{
    return (int)round(Material.SpecularOld.x);
}

int GetMaterialUVScaleMode()
{
    return (int)round(Material.SpecularOld.y);
}

float GetMaterialScale3D()
{
    float s = Material.SpecularOld.z;
    return (s > 0.001f) ? s : 1.0f;
}

int GetMaterialSeed()
{
    int s = (int)round(Material.SpecularOld.w);
    return (s != 0) ? s : 1337;
}

// =====================================================================================
// 2D & 3D Hash and Gradient Noise (Perlin / FBM / Voronoi)
// =====================================================================================
uint Hash2D(int x, int y, int seed)
{
    uint h = (uint)(x * 374761393 + y * 668265263 + seed * 1442695041);
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

uint Hash3D(int x, int y, int z, int seed)
{
    uint h = (uint)(x * 374761393 + y * 668265263 + z * 2147483647 + seed * 1442695041);
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

float FadePerlin(float t)
{
    return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

float GradientNoise2D(float x, float y, int periodX, int periodY, int seed)
{
    int px = max(1, periodX);
    int py = max(1, periodY);
    int ix = (int)floor(x);
    int iy = (int)floor(y);
    float fx = x - (float)ix;
    float fy = y - (float)iy;
    int x0 = ((ix % px) + px) % px;
    int y0 = ((iy % py) + py) % py;
    int x1 = (x0 + 1) % px;
    int y1 = (y0 + 1) % py;

    float u = FadePerlin(fx);
    float v = FadePerlin(fy);

    uint h00 = Hash2D(x0, y0, seed);
    uint h10 = Hash2D(x1, y0, seed);
    uint h01 = Hash2D(x0, y1, seed);
    uint h11 = Hash2D(x1, y1, seed);

    float a00 = (float)(h00 & 0xFFFFu) / 65535.0f * 2.0f * PROC_PI;
    float a10 = (float)(h10 & 0xFFFFu) / 65535.0f * 2.0f * PROC_PI;
    float a01 = (float)(h01 & 0xFFFFu) / 65535.0f * 2.0f * PROC_PI;
    float a11 = (float)(h11 & 0xFFFFu) / 65535.0f * 2.0f * PROC_PI;

    float d00 = cos(a00) * fx + sin(a00) * fy;
    float d10 = cos(a10) * (fx - 1.0f) + sin(a10) * fy;
    float d01 = cos(a01) * fx + sin(a01) * (fy - 1.0f);
    float d11 = cos(a11) * (fx - 1.0f) + sin(a11) * (fy - 1.0f);

    return lerp(lerp(d00, d10, u), lerp(d01, d11, u), v);
}

float Grad3D(uint h, float dx, float dy, float dz)
{
    float theta = (float)(h & 0xFFFFu) / 65535.0f * 2.0f * PROC_PI;
    float z = ((float)((h >> 16) & 0xFFFFu) / 65535.0f) * 2.0f - 1.0f;
    float r = sqrt(max(0.0f, 1.0f - z * z));
    return (r * cos(theta)) * dx + (r * sin(theta)) * dy + z * dz;
}

float GradientNoise3D(float x, float y, float z, int seed)
{
    int ix = (int)floor(x);
    int iy = (int)floor(y);
    int iz = (int)floor(z);
    float fx = x - (float)ix;
    float fy = y - (float)iy;
    float fz = z - (float)iz;
    float u = FadePerlin(fx);
    float v = FadePerlin(fy);
    float w = FadePerlin(fz);

    float n000 = Grad3D(Hash3D(ix,     iy,     iz,     seed), fx,        fy,        fz);
    float n100 = Grad3D(Hash3D(ix + 1, iy,     iz,     seed), fx - 1.0f, fy,        fz);
    float n010 = Grad3D(Hash3D(ix,     iy + 1, iz,     seed), fx,        fy - 1.0f, fz);
    float n110 = Grad3D(Hash3D(ix + 1, iy + 1, iz,     seed), fx - 1.0f, fy - 1.0f, fz);
    float n001 = Grad3D(Hash3D(ix,     iy,     iz + 1, seed), fx,        fy,        fz - 1.0f);
    float n101 = Grad3D(Hash3D(ix + 1, iy,     iz + 1, seed), fx - 1.0f, fy,        fz - 1.0f);
    float n011 = Grad3D(Hash3D(ix,     iy + 1, iz + 1, seed), fx,        fy - 1.0f, fz - 1.0f);
    float n111 = Grad3D(Hash3D(ix + 1, iy + 1, iz + 1, seed), fx - 1.0f, fy - 1.0f, fz - 1.0f);

    float nx00 = lerp(n000, n100, u);
    float nx10 = lerp(n010, n110, u);
    float nx01 = lerp(n001, n101, u);
    float nx11 = lerp(n011, n111, u);
    return lerp(lerp(nx00, nx10, v), lerp(nx01, nx11, v), w);
}

// =====================================================================================
// 3D Coordinate Projection Helpers
// =====================================================================================
float3 MapUVTo3D(float2 uv, int spaceMode)
{
    if (spaceMode == 1)
    {
        float phi   = uv.x * 2.0f * PROC_PI;
        float theta = saturate(uv.y) * PROC_PI;
        float sinT  = sin(theta);
        return float3(sinT * cos(phi), cos(theta), sinT * sin(phi));
    }
    else if (spaceMode == 2)
    {
        float phi   = uv.x * 2.0f * PROC_PI;
        float theta = saturate(uv.y) * PROC_PI;
        float sinT  = sin(theta);
        float3 d    = float3(sinT * cos(phi), cos(theta), sinT * sin(phi));
        float maxA  = max(max(abs(d.x), abs(d.y)), max(abs(d.z), 0.0001f));
        return d / maxA;
    }
    return float3(uv.x, uv.y, 0.0f);
}

float2 Dir3DToSphericalUV(float3 dir)
{
    float len = length(dir);
    float3 n = (len >= 0.0001f) ? (dir / len) : float3(0.0f, 1.0f, 0.0f);
    float phi = atan2(n.z, n.x);
    if (phi < 0.0f) phi += 2.0f * PROC_PI;
    float u = frac(phi / (2.0f * PROC_PI));
    float v = saturate(acos(clamp(n.y, -1.0f, 1.0f)) / PROC_PI);
    return (len >= 0.0001f) ? float2(u, v) : float2(0.0f, 0.0f);
}

// =====================================================================================
// Procedural Node Functions (Callable directly inside Pixel Shaders)
// =====================================================================================
float Node_PerlinFBM(float2 uv, float3 pos3D, float scaleX, float scaleY, int octaves, float persistence, float lacunarity, int seed, bool ridged)
{
    float total = 0.0f;
    float amplitude = 1.0f;
    float maxAmp = 0.0f;
    float freqX = max(0.5f, scaleX);
    float freqY = max(0.5f, scaleY);
    int oct = clamp(octaves, 1, 8);
    [loop]
    for (int i = 0; i < oct; i++)
    {
        float n = GradientNoise3D(
            (pos3D.x + 3.7f) * freqX * 0.5f,
            (pos3D.y + 3.7f) * freqY * 0.5f,
            (pos3D.z + 3.7f) * freqX * 0.5f,
            seed + i * 101
        );
        if (ridged)
        {
            n = 1.0f - abs(n * 1.414f);
            n = saturate(n);
            n = n * n;
        }
        else
        {
            n = saturate(n * 0.7071f + 0.5f);
        }
        total += n * amplitude;
        maxAmp += amplitude;
        amplitude *= persistence;
        freqX *= lacunarity;
        freqY *= lacunarity;
    }
    return (maxAmp > 0.0f) ? saturate(total / maxAmp) : 0.0f;
}

float Node_Voronoi(float2 uv, float3 pos3D, float scale, float jitter, int mode, int seed, bool invertFlag)
{
    float f1 = 1e9f;
    float f2 = 1e9f;
    float bestCellVal = 0.0f;
    float freq = max(0.5f, scale) * 0.5f;
    float3 sp = (pos3D + 5.0f) * freq;
    int ix = (int)floor(sp.x);
    int iy = (int)floor(sp.y);
    int iz = (int)floor(sp.z);
    for (int dz = -1; dz <= 1; dz++)
    {
        for (int dy = -1; dy <= 1; dy++)
        {
            for (int dx = -1; dx <= 1; dx++)
            {
                int cx = ix + dx;
                int cy = iy + dy;
                int cz = iz + dz;
                uint h1 = Hash3D(cx, cy, cz, seed);
                uint h2 = Hash3D(cx, cy, cz, seed + 7919);
                float rx = (float)(h1 & 0xFFFFu) / 65535.0f;
                float ry = (float)((h1 >> 16) & 0xFFFFu) / 65535.0f;
                float rz = (float)(h2 & 0xFFFFu) / 65535.0f;
                float3 fp = float3(cx, cy, cz) + 0.5f + (float3(rx, ry, rz) - 0.5f) * jitter;
                float dist = length(fp - sp);
                if (dist < f1)
                {
                    f2 = f1;
                    f1 = dist;
                    bestCellVal = (float)((h1 >> 8) & 0xFFFFu) / 65535.0f;
                }
                else if (dist < f2)
                {
                    f2 = dist;
                }
            }
        }
    }
    float val = saturate(f1);
    if (mode == 1) val = saturate((f2 - f1) * 1.5f);
    else if (mode == 2) val = saturate(bestCellVal);
    return invertFlag ? (1.0f - val) : val;
}

float4 Node_Checker(float2 uv, float scaleX, float scaleY, bool gridMode, float lineWidth, float3 colorA, float3 colorB)
{
    float sx = uv.x * max(1.0f, scaleX);
    float sy = uv.y * max(1.0f, scaleY);
    float3 col = colorA;
    if (gridMode)
    {
        float fx = frac(sx); if (fx < 0.0f) fx += 1.0f;
        float fy = frac(sy); if (fy < 0.0f) fy += 1.0f;
        float lw = saturate(lineWidth * 0.5f);
        bool isLine = (fx < lw || fx > 1.0f - lw || fy < lw || fy > 1.0f - lw);
        col = isLine ? colorB : colorA;
    }
    else
    {
        int cx = (int)floor(sx);
        int cy = (int)floor(sy);
        col = (((cx + cy) & 1) == 0) ? colorA : colorB;
    }
    return float4(col, 1.0f);
}

float4 Node_Brick(float2 uv, float scaleX, float scaleY, float mortar, float offset, float variation, int seed, float3 colorBrick, float3 colorMortar)
{
    float cols = max(1.0f, round(scaleX));
    float rows = max(1.0f, round(scaleY));
    float m = saturate(mortar);
    float ry = uv.y * rows;
    int rowIdx = (int)floor(ry);
    float rx = uv.x * cols + (((rowIdx & 1) != 0) ? offset : 0.0f);
    int colIdx = (int)floor(rx);
    float fx = frac(rx); if (fx < 0.0f) fx += 1.0f;
    float fy = frac(ry); if (fy < 0.0f) fy += 1.0f;
    float mx = m * 0.5f;
    float my = m * (cols / rows) * 0.5f;
    if (fx < mx || fx > 1.0f - mx || fy < my || fy > 1.0f - my)
    {
        return float4(colorMortar, 1.0f);
    }
    uint bh = Hash2D(colIdx, rowIdx, seed);
    float var = (((float)(bh & 0xFFFFu) / 65535.0f) - 0.5f) * 2.0f * variation;
    return float4(saturate(colorBrick + var), 1.0f);
}

float4 Node_Gradient(float2 uv, float3 pos3D, int mode, float angleDeg, float repeatScale, float3 colorA, float3 colorB)
{
    float rad = angleDeg * PROC_PI / 180.0f;
    float ca = cos(rad);
    float sa = sin(rad);
    float t = 0.0f;
    if (mode == 0)
    {
        float proj = (-pos3D.y * ca + pos3D.x * sa) * 0.5f + 0.5f;
        t = (repeatScale <= 1.05f) ? saturate(proj * repeatScale) : frac(proj * repeatScale);
    }
    else
    {
        float2 centerXZ = float2(sa * 0.65f, (1.0f - ca) * 0.45f);
        float d = length(pos3D.xz - centerXZ);
        float scaled = d * max(0.1f, repeatScale);
        t = (repeatScale <= 1.05f) ? saturate(scaled) : frac(scaled);
    }
    return float4(lerp(colorA, colorB, t), 1.0f);
}

float4 Node_Blend(float4 a, float4 b, float opacity, float maskVal, int mode)
{
    float factor = saturate(opacity) * maskVal;
    float3 res = b.rgb;
    if (mode == 1) res = a.rgb * b.rgb;
    else if (mode == 2) res = saturate(a.rgb + b.rgb);
    else if (mode == 3) res = saturate(a.rgb - b.rgb);
    else if (mode == 4) res = 1.0f - (1.0f - a.rgb) * (1.0f - b.rgb);
    else if (mode == 5) res = (a.rgb < 0.5f) ? (2.0f * a.rgb * b.rgb) : (1.0f - 2.0f * (1.0f - a.rgb) * (1.0f - b.rgb));
    else if (mode == 6) res = max(a.rgb, b.rgb);
    else if (mode == 7) res = min(a.rgb, b.rgb);
    return float4(saturate(lerp(a.rgb, res, factor)), 1.0f);
}

float4 Node_ColorRamp(float t, bool threeStops, float stop0, float stop1, float stop2, float3 colA, float3 colB, float3 colC)
{
    float p0 = saturate(stop0);
    float p1 = max(p0, saturate(stop1));
    float p2 = max(p1, saturate(stop2));
    float f2 = saturate((t - p0) / max(0.0001f, p2 - p0));
    float fA = saturate((t - p0) / max(0.0001f, p1 - p0));
    float fB = saturate((t - p1) / max(0.0001f, p2 - p1));
    float3 c3 = (t <= p1) ? lerp(colA, colB, fA) : lerp(colB, colC, fB);
    float3 res = threeStops ? c3 : lerp(colA, colC, f2);
    return float4(res, 1.0f);
}

float4 Node_Levels(float4 inCol, float brightness, float contrast, float gammaVal, bool invertFlag)
{
    float3 v = inCol.rgb;
    if (invertFlag) v = 1.0f - v;
    v = saturate((v - 0.5f) * contrast + 0.5f + brightness);
    float invGamma = 1.0f / max(0.05f, gammaVal);
    v = saturate(pow(max(v, 0.0f), invGamma));
    return float4(v, 1.0f);
}

// =====================================================================================
// UE5-Style 2D UV Wrap / Clamp / Mirror / Stochastic Random Tiling Sampler
// =====================================================================================
float4 SampleTexture2DWithWrapMode(Texture2D<float4> tex, float2 rawUV, int wrapMode, int seed)
{
    float2 dx = ddx(rawUV);
    float2 dy = ddy(rawUV);
    float4 result = tex.Sample(Sampler, rawUV);

    if (wrapMode == 1)
    {
        result = tex.Sample(SamplerClamp, saturate(rawUV));
    }
    else if (wrapMode == 2)
    {
        float2 t = frac(rawUV * 0.5f) * 2.0f;
        float2 mUV = 1.0f - abs(t - 1.0f);
        result = tex.SampleGrad(SamplerClamp, mUV, dx, dy);
    }
    else if (wrapMode == 3)
    {
        float2 iuv = floor(rawUV);
        float2 fuv = frac(rawUV);
        float2 w = smoothstep(0.15f, 0.85f, fuv);

        int ix = (int)iuv.x;
        int iy = (int)iuv.y;

        uint h00 = Hash2D(ix,     iy,     seed);
        uint h10 = Hash2D(ix + 1, iy,     seed);
        uint h01 = Hash2D(ix,     iy + 1, seed);
        uint h11 = Hash2D(ix + 1, iy + 1, seed);

        float2 off00 = float2((float)(h00 & 0xFFu), (float)((h00 >> 8) & 0xFFu)) / 255.0f;
        float2 off10 = float2((float)(h10 & 0xFFu), (float)((h10 >> 8) & 0xFFu)) / 255.0f;
        float2 off01 = float2((float)(h01 & 0xFFu), (float)((h01 >> 8) & 0xFFu)) / 255.0f;
        float2 off11 = float2((float)(h11 & 0xFFu), (float)((h11 >> 8) & 0xFFu)) / 255.0f;

        float2 s00 = ((h00 & 0x10000u) != 0u) ? float2(-1.0f, 1.0f) : float2(1.0f, 1.0f);
        float2 s10 = ((h10 & 0x10000u) != 0u) ? float2(-1.0f, 1.0f) : float2(1.0f, 1.0f);
        float2 s01 = ((h01 & 0x10000u) != 0u) ? float2(-1.0f, 1.0f) : float2(1.0f, 1.0f);
        float2 s11 = ((h11 & 0x10000u) != 0u) ? float2(-1.0f, 1.0f) : float2(1.0f, 1.0f);

        float4 c00 = tex.SampleGrad(Sampler, rawUV * s00 + off00, dx * s00, dy * s00);
        float4 c10 = tex.SampleGrad(Sampler, rawUV * s10 + off10, dx * s10, dy * s10);
        float4 c01 = tex.SampleGrad(Sampler, rawUV * s01 + off01, dx * s01, dy * s01);
        float4 c11 = tex.SampleGrad(Sampler, rawUV * s11 + off11, dx * s11, dy * s11);

        result = lerp(lerp(c00, c10, w.x), lerp(c01, c11, w.x), w.y);
    }

    return result;
}

// =====================================================================================
// 3D World-Scale Triplanar PNG Mapping with 3D Noise Organic Anti-Tiling
// =====================================================================================
float4 SampleTexture3DTriplanar(Texture2D<float4> tex, float3 scaledPos3D, float3 localNormal, float2 tiling, float2 offset, int wrapMode, int seed)
{
    float3 blendW = pow(abs(localNormal), 4.0f);
    float sumW = max(0.0001f, blendW.x + blendW.y + blendW.z);
    blendW /= sumW;

    // Optional subtle 3D noise warp when using Stochastic mode (wrapMode == 3)
    float3 p = scaledPos3D;
    if (wrapMode == 3)
    {
        float nx = GradientNoise3D(p.x * 0.7f, p.y * 0.7f, p.z * 0.7f, seed);
        float ny = GradientNoise3D(p.x * 0.7f + 19.3f, p.y * 0.7f + 7.1f, p.z * 0.7f + 31.7f, seed + 101);
        float nz = GradientNoise3D(p.x * 0.7f + 43.1f, p.y * 0.7f + 53.9f, p.z * 0.7f + 11.3f, seed + 202);
        p += float3(nx, ny, nz) * 0.18f;
    }

    float2 uvX = p.zy * tiling + offset;
    float2 uvY = p.xz * tiling + offset;
    float2 uvZ = p.xy * tiling + offset;

    float4 colX = SampleTexture2DWithWrapMode(tex, uvX, wrapMode, seed + 17);
    float4 colY = SampleTexture2DWithWrapMode(tex, uvY, wrapMode, seed + 53);
    float4 colZ = SampleTexture2DWithWrapMode(tex, uvZ, wrapMode, seed + 97);

    return colX * blendW.x + colY * blendW.y + colZ * blendW.z;
}

// =====================================================================================
// Built-in 3D Procedural Texture for Mode 3 in GeometryPS.hlsl
// Option B: 3D Voxel Checker/Grid x 3D Perlin FBM Hybrid (Engine Standard 3D Solid Test)
// =====================================================================================
void EvaluateDefaultProcedural3D(
    float3 pos3D,
    int seed,
    out float4 outAlbedo,
    out float3 outLocalNormalOffset,
    out float3 outARM)
{
    // 1. 3D Voxel Coordinate & 3D Parity Checker
    float3 gridPos = pos3D * 2.0f;
    int3 voxel = (int3)floor(gridPos);
    float3 cellUVW = frac(gridPos);

    // 3D Checkerboard parity: alternates across X, Y, and Z axes in 3D space
    int parity = (voxel.x + voxel.y + voxel.z) & 1;
    float voxelRand = Hash3D(voxel.x, voxel.y, voxel.z, seed);

    // 2. Multi-Octave 3D Perlin FBM & 3D Ridged FBM (Continuous across voxel boundaries)
    float fbmBase   = Node_PerlinFBM(float2(0.0f, 0.0f), pos3D, 2.5f, 1.8f, 4, 0.5f, 2.0f, seed, false);
    float fbmRidge  = Node_PerlinFBM(float2(0.0f, 0.0f), pos3D, 6.0f, 2.0f, 3, 0.5f, 2.0f, seed + 137, true);
    float noiseVal  = saturate(fbmBase * 0.70f + fbmRidge * 0.30f);

    // Subtle 3D iso-surface contour rings from continuous 3D FBM
    float contourWave = sin(fbmBase * 18.0f * PROC_PI);
    float contourLine = smoothstep(0.82f, 0.98f, contourWave) * 0.18f;

    // 3. 3D Voxel Palette (Even vs Odd 3D cells + per-voxel hash variation + 3D FBM modulation)
    float3 evenDark  = float3(0.14f, 0.20f, 0.27f);
    float3 evenLight = float3(0.30f, 0.48f, 0.62f);
    float3 oddDark   = float3(0.22f, 0.23f, 0.26f);
    float3 oddLight  = float3(0.52f, 0.54f, 0.58f);

    float3 cellColorEven = lerp(evenDark, evenLight, noiseVal) * (0.90f + 0.18f * voxelRand);
    float3 cellColorOdd  = lerp(oddDark,  oddLight,  noiseVal) * (0.90f + 0.18f * voxelRand);
    float3 baseCol = (parity != 0) ? cellColorEven : cellColorOdd;
    baseCol += contourLine * float3(0.18f, 0.26f, 0.32f);

    // 4. 3D Spatial Grid Planes & Sub-Voxel Ticks (X, Y, Z slicing planes)
    float3 distToPlane = min(cellUVW, 1.0f - cellUVW);
    float3 majorPlane  = 1.0f - smoothstep(0.012f, 0.042f, distToPlane);

    float3 subUVW      = frac(gridPos * 4.0f);
    float3 distToSub   = min(subUVW, 1.0f - subUVW);
    float3 subPlane    = (1.0f - smoothstep(0.025f, 0.075f, distToSub)) * 0.25f;

    float majorMask = saturate(max(majorPlane.x, max(majorPlane.y, majorPlane.z)));
    float subMask   = saturate(max(subPlane.x,   max(subPlane.y,   subPlane.z)));

    // Subtle XYZ axis color coding on the 3D slicing planes (X=Red-Orange, Y=Green-Teal, Z=Blue-Cyan)
    float3 axisTint = majorPlane.x * float3(0.95f, 0.38f, 0.28f)
                    + majorPlane.y * float3(0.30f, 0.88f, 0.52f)
                    + majorPlane.z * float3(0.28f, 0.68f, 0.98f);
    float3 gridLineColor = lerp(float3(0.85f, 0.90f, 0.95f), saturate(axisTint), 0.55f);

    // Darken groove slightly around major plane, then highlight the crisp center line
    float grooveMask = (1.0f - smoothstep(0.02f, 0.08f, min(distToPlane.x, min(distToPlane.y, distToPlane.z))));
    baseCol *= lerp(1.0f, 0.65f, grooveMask);
    baseCol = lerp(baseCol, baseCol * 1.25f, subMask * (1.0f - majorMask));
    baseCol = lerp(baseCol, gridLineColor, majorMask * 0.85f);

    outAlbedo = float4(saturate(baseCol), 1.0f);

    // 5. 3D Normal Offset (3D Voxel Bevel Groove + 3D Perlin FBM Finite-Difference Gradient)
    float eps = 0.02f;
    float nx = Node_PerlinFBM(float2(0.0f, 0.0f), pos3D + float3(eps, 0.0f, 0.0f), 2.5f, 1.8f, 3, 0.5f, 2.0f, seed, false) - fbmBase;
    float ny = Node_PerlinFBM(float2(0.0f, 0.0f), pos3D + float3(0.0f, eps, 0.0f), 2.5f, 1.8f, 3, 0.5f, 2.0f, seed, false) - fbmBase;
    float nz = Node_PerlinFBM(float2(0.0f, 0.0f), pos3D + float3(0.0f, 0.0f, eps), 2.5f, 1.8f, 3, 0.5f, 2.0f, seed, false) - fbmBase;
    float3 fbmGrad = -float3(nx, ny, nz) / eps * 0.14f;

    // 3D Voxel edge bevel normal pointing away from the nearest cell boundary plane
    float3 bevelDir = (cellUVW - 0.5f) * 2.0f;
    float3 bevelStrength = smoothstep(0.84f, 0.98f, abs(bevelDir));
    float3 voxelBevel = sign(bevelDir) * bevelStrength * 0.35f;

    outLocalNormalOffset = fbmGrad + voxelBevel;

    // 6. ARM (AO darkened in 3D voxel grooves, Roughness modulated by 3D FBM & Checker, Metallic accent on grid)
    float ao = lerp(1.0f, 0.72f, grooveMask);
    float cellRough = lerp(Material.Roughness * 0.75f + 0.10f, saturate(Material.Roughness * 1.15f), 1.0f - noiseVal);
    if (parity == 0)
    {
        cellRough = saturate(cellRough + 0.10f);
    }
    float rough = lerp(cellRough, 0.22f, majorMask);
    float metal = lerp(Material.Metallic, saturate(Material.Metallic + 0.35f), majorMask);
    outARM = float3(ao, saturate(rough), saturate(metal));
}

#endif // _PROCEDURAL_COMMON_HLSL
