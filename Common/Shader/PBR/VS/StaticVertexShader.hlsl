#include "../../CommonSharedVS.hlsli"

VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output = (VS_OUTPUT) 0;

    float4 worldPos = float4(input.Pos, 1.0f);
    
    output.Pos = mul(worldPos, mWorld);
    
    output.W_Pos = output.Pos;
    output.Pos = mul(output.Pos, mView);
    output.Pos = mul(output.Pos, mProjection);
    
    output.Norm = normalize(mul(input.Norm, (float3x3) mNormalMatrix));
    output.Tan = normalize(mul(input.Tan, (float3x3) mNormalMatrix));
    output.BiTan = normalize(cross(output.Norm, output.Tan));
    output.Tex = input.Tex;
    
    // 현재 위치를 ShadowMap 위치로 변환
    output.S_Pos = mul(float4(output.W_Pos.xyz, 1.0f), mShadowView);
    output.S_Pos = mul(output.S_Pos, mShadowProjection);
    
    return output;
}