#include "Common.hlsl"

struct VS_OUT{
    float4 Position : SV_POSITION;
};

VS_OUT main(VS_IN input){
    VS_OUT output;
    // オブジェクトのローカル座標からワールド座標へ変換
    float4 worldPos = mul(float4(input.Position, 1.0f), World);
    // ワールド座標からライト空間（正射影）へ変換
    output.Position = mul(worldPos, Light.LightVP);
    return output;
}