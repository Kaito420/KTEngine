#include "Common.hlsl"

struct VS_OUT{
    float4 Position : SV_POSITION;
};

VS_OUT main(VS_IN input){
    VS_OUT output;
    float4 localPos = float4(input.Position, 1.0f);
    float4 dummyNorm = float4(input.Normal, 0.0f);
    ApplySkinning(localPos, dummyNorm, input.BoneIndices, input.BoneWeights);

    // オブジェクトのローカル座標からワールド座標へ変換
    float4 worldPos = mul(localPos, World);
    // ワールド座標からライト空間（正射影）へ変換
    output.Position = mul(worldPos, Light.LightVP);
    return output;
}