#include "Common.hlsl"

PS_OUTPUT main(PS_INPUT input)
{
    PS_OUTPUT output;
    
    float4 baseColor = TextureBaseColor.Sample(Sampler, input.TexCoord);
    float4 normal = TextureNormal.Sample(Sampler, input.TexCoord);

    // 背景ベクトル（法線が(0,0,0)になっている箇所）を除外
    if (dot(normal.xyz, normal.xyz) < 0.01f) {
        output.Color = baseColor;
        return output;
    }

    float4 position = TexturePosition.Sample(Sampler, input.TexCoord);
    float4 materialMetallicParam = TextureMaterialMetallic.Sample(Sampler, input.TexCoord);
    float4 materialSpecularParam = TextureMaterialSpecular.Sample(Sampler, input.TexCoord);
    float4 materialRoughnessParam = TextureMaterialRoughness.Sample(Sampler, input.TexCoord);
    float3 lightDir = normalize(LightDirection.xyz); // 光源の方向
    float3 n = normalize(normal.xyz); // 法線ベクトル
    output.Color = baseColor; // 初期化
    output.Color.a = 1.0f; // アルファは不透明に設定
    // 視線ベクトル（ピクセル→カメラ）
    float3 eye = CameraPosition.xyz - position.xyz;
    eye = normalize(eye);
    // 視線の反射ベクトル
    float3 refEye = reflect(-eye, n);
    
    // // BaseDiffuse
    // float ndotl = dot(n, lightDir);
    // float diffuse = 0.0f;
    // if (DiffuseModel == 0) {
    //     diffuse = max(ndotl, 0.0f); // Lambert
    // } 
    // else if (DiffuseModel == 1) {
    //     diffuse = max(0.5f * ndotl + 0.5f, 0.0f); // half-Lambert
    // }
    // else if (DiffuseModel == 2) { // normalized-Lambert
    //     diffuse = max(ndotl, 0.0f);
    //     diffuse /= PI;
    // }

    // // Shading Style
    // if (ShadingModel == 1) {
    // // トゥーンの場合は明暗を階調化する
    // if (diffuse > 0.7f)      diffuse = 1.0f;
    // else if (diffuse > 0.4f) diffuse = 0.7f;
    // else if (diffuse > 0.2f) diffuse = 0.4f;
    // else                     diffuse = 0.2f;
    // }

    // float3 finalColor = baseColor.rgb * diffuse * LightColor.rgb;

    // // Specular
    // if (SpecularModel == 1) {
    //     float3 SpecularColor = float3(1.0f, 1.0f, 1.0f); // 白色のスペキュラ
    //     float SpecularPower = 20.0f;
    //     float specular = dot(refEye, lightDir);
    //     specular = pow(saturate(specular), SpecularPower);
    //     // // トゥーンスタイルの場合はハイライトもパキッと二値化する
    //     // if (ShadingModel == 1) {
    //     //     spec = step(0.5f, spec); 
    //     // }
    
    //     finalColor += SpecularColor.xyz * specular;;
    // }

    // // Rim Light

    // if (RimLightModel == 1) {
    //     float rim = 1.0f - max(dot(n, eye), 0.0f);
    //     rim = smoothstep(0.6f, 1.0f, rim); // リムライトの滑らかさを調整
    //     rim = pow(rim, RimPower); // リムライトの強さを調整
        
    //     // // トゥーンスタイルの場合はリムライトも二値化する
    //     // if (ShadingModel == 1) {
    //     //     rim = step(0.5f, rim);
    //     // }
        
    //     finalColor += RimColor * rim;
    // }

    // output.Color = float4(finalColor, baseColor.a);

    // return output;

    // BRDFを使う

    // 拡散反射
    float3 diffuse = 0.0f;
    {
        // 正規化ランバート
        float3 light = LightColor.rgb * saturate(dot(lightDir, n));
        diffuse = light * baseColor.rgb / PI;
    }

    // 鏡面反射
    float3 specular = 0.0f;
    {
        // 独自のパラメータ設定 (例として定数を直接書き込む)
        float roughness = materialRoughnessParam.x; // ラフネス (0.0 ～ 1.0)
        float metallic = materialMetallicParam.x;  // 金属度 (0.0 ～ 1.0)

        // F0の計算: 非金属のベース反射率は0.04(4%)とする。金属の場合はベースカラーをそのまま反射色として使う
        float3 F0 = lerp(float3(0.04f, 0.04f, 0.04f), baseColor.rgb, metallic);

        // ハーフベクトル
        float3 h = normalize(lightDir + eye); 

        // 各種内積（マイナスにならないよう 0.0f でクランプ）
        float ndotl = max(dot(n, lightDir), 0.0f);
        float ndotv = max(dot(n, eye), 0.0f);
        float ndoth = max(dot(n, h), 0.0f);
        float vdoth = max(dot(eye, h), 0.0f);

        // クックトランスマイクロファセット
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

        // F: Schlickの近似
        float3 f = F0 + (1.0f - F0) * pow(1.0f - vdoth, 5.0f);

        // 最終的なスペキュラBRDF (0除算を防ぐため分母に小さな値 0.0001f を足す)
        float3 brdfSpecular = (d * g * f) / max(4.0f * ndotl * ndotv, 0.0001f);
        
        // 光源の色や方向の強さを加算する
        specular = brdfSpecular * LightColor.rgb * ndotl;
    }
    
    output.Color.rgb = diffuse + specular;
    output.Color.a = 1.0f;
    return output;

}