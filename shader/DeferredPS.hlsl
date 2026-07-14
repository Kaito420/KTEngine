#include "Common.hlsl"

#define PI 3.14159265f

PS_OUTPUT main(PS_IN input)
{
    PS_OUTPUT output;
    
    float4 baseColor = TextureBaseColor.Sample(Sampler, input.TexCoord);
    float4 normal = TextureNormal.Sample(Sampler, input.TexCoord);

    // 背景（法線が0）の場合はライティング計算をスキップして背景色を出力
    if (dot(normal.xyz, normal.xyz) < 0.01f) {
        output.Color = baseColor;
        return output;
    }

    float4 position = TexturePosition.Sample(Sampler, input.TexCoord);
    float4 materialARM = TextureMaterialARM.Sample(Sampler, input.TexCoord);
    float ao = materialARM.r;
    float roughness = materialARM.g;
    float metallic = materialARM.b;
    int shadingModel = (int)(materialARM.a * 255.0f + 0.5f);

    float3 lightDir = -normalize(Light.Direction.xyz); // 光源の方向
    float3 n = normalize(normal.xyz); // 法線ベクトル
    float3 eye = normalize(CameraPosition.xyz - position.xyz); // 視線ベクトル
    float3 h = normalize(lightDir + eye); // ハーフベクトル
    float3 refEye = reflect(-eye, n); // 反射ベクトル

    // --- シャドウ判定 ---
    float shadowFactor = 1.0f;
    
    // 1. ワールド座標をライト投影空間へ変換
    float4 lightSpacePos = mul(float4(position.xyz, 1.0f), Light.LightVP);
    float3 projCoords = lightSpacePos.xyz / lightSpacePos.w;
    
    // 2. 射影空間 [-1, 1] から テクスチャUV空間 [0, 1] へ変換 (DirectX用にY反転)
    float2 shadowUV;
    shadowUV.x = projCoords.x * 0.5f + 0.5f;
    shadowUV.y = -projCoords.y * 0.5f + 0.5f;
    
    // 3. 影テクスチャの範囲内のみでサンプリング比較
    if (shadowUV.x >= 0.0f && shadowUV.x <= 1.0f && shadowUV.y >= 0.0f && shadowUV.y <= 1.0f) {
        // デプス比較バイアス（シャドウアクネ防止）
        float bias = 0.002f;
        shadowFactor = TextureShadowMap.SampleCmpLevelZero(ShadowSampler, float3(shadowUV, 0.0f), projCoords.z - bias);
    }

    float3 finalColor = float3(0, 0, 0);

    // 1. Toon Shading (shadingModel == 1 の場合) または ベース描画 (PBR)
    if (shadingModel == 1) { // Toon Shading
        // Diffuse
        float ndotl = dot(n, lightDir);
        float diffuse = max(ndotl, 0.0f);
        
        if (Light.DiffuseModel == 1) { // half-Lambert
            diffuse = max(0.5f * ndotl + 0.5f, 0.0f);
        }
        
        // トゥーン段差
        if (diffuse > 0.7f)      diffuse = 1.0f;
        else if (diffuse > 0.4f) diffuse = 0.7f;
        else if (diffuse > 0.2f) diffuse = 0.4f;
        else                     diffuse = 0.2f;
        
        // 影の影響を乗算
        diffuse *= shadowFactor;
        
        finalColor = baseColor.rgb * diffuse * Light.Diffuse.rgb * Light.Intensity;

        // Specular (Toon Specular)
        if (Light.SpecularModel == 1) {
            float3 specularColor = float3(1.0f, 1.0f, 1.0f);
            float specularPower = 20.0f;
            float specular = dot(refEye, lightDir);
            specular = pow(saturate(specular), specularPower);
            specular = step(0.5f, specular); // トゥーンハイライト
            // 影の影響を乗算
            specular *= shadowFactor;
            finalColor += specularColor * specular * Light.Diffuse.rgb * Light.Intensity;
        }
    }
    else { // ベース描画 (PBR)
        // Diffuse (Lambert または Half-Lambert の選択に対応)
        float ndotl_diffuse = dot(n, lightDir);
        float diffuseTerm = max(ndotl_diffuse, 0.0f);
        if (Light.DiffuseModel == 1) { // Half-Lambert
            diffuseTerm = max(0.5f * ndotl_diffuse + 0.5f, 0.0f);
        } else if (Light.DiffuseModel == 2) { // Normalized-Lambert
            diffuseTerm = max(ndotl_diffuse, 0.0f) / PI;
        }
        
        float3 diffuseColor = Light.Diffuse.rgb * Light.Intensity * diffuseTerm * baseColor.rgb;

        // Specular (GGX, Cook-Torrance BRDF)
        float3 specularColor = float3(0.0f, 0.0f, 0.0f);
        if (Light.SpecularModel == 1) {
            float3 F0 = lerp(float3(0.04f, 0.04f, 0.04f), baseColor.rgb, metallic);

            float ndotl = max(dot(n, lightDir), 0.0f);
            float ndotv = max(dot(n, eye), 0.0f);
            float ndoth = max(dot(n, h), 0.0f);
            float vdoth = max(dot(eye, h), 0.0f);

            // D: Trowbridge-Reitz GGX
            float alpha = roughness * roughness;
            float alpha2 = alpha * alpha;
            float denom = (ndoth * ndoth) * (alpha2 - 1.0f) + 1.0f;
            float d = alpha2 / (PI * denom * denom);

            // G: Schlick-GGX (Smith)
            float k = (roughness + 1.0f) * (roughness + 1.0f) / 8.0f;
            float gl = ndotl / (ndotl * (1.0f - k) + k);
            float gv = ndotv / (ndotv * (1.0f - k) + k);
            float g = gl * gv;

            // F: Schlick
            float3 f = F0 + (1.0f - F0) * pow(1.0f - vdoth, 5.0f);

            float3 brdfSpecular = (d * g * f) / max(4.0f * ndotl * ndotv, 0.0001f);
            specularColor = brdfSpecular * Light.Diffuse.rgb * Light.Intensity * ndotl;
        }
        
        // 影の影響を乗算
        finalColor = (diffuseColor + specularColor) * shadowFactor;
    }

    // 2. Rim Light (リムライト)
    if (Light.RimLightModel == 1) {
        float rim = 1.0f - max(dot(n, eye), 0.0f);
        rim = smoothstep(0.6f, 1.0f, rim);
        rim = pow(rim, Light.RimPower);
        
        if (shadingModel == 1) { // Toon Rim
            rim = step(0.5f, rim);
        }
        
        finalColor += Light.RimColor.rgb * rim * Light.Intensity;
    }

    // 3. Ambient (環境光) の付加
    finalColor += baseColor.rgb * Light.Ambient.rgb * Light.AmbientIntensity * ao;

    // 4. Exposure (露出調整) の適用
    finalColor *= Light.Exposure;

    // 5. ACES Tone Mapping
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    finalColor = saturate((finalColor * (a * finalColor + b)) / (finalColor * (c * finalColor + d) + e));

    output.Color = float4(finalColor, baseColor.a);
    return output;
}
