#include "SkyBoxShared.hlsli"

float3 ComputeCamDir(float2 ndc)
{
    float3x3 invRot = transpose((float3x3) mView);
    
    // Perspective: ÇÈ¼¿º° ray
    float4 clip = float4(ndc, 1.0f, 1.0f);
    float4 viewPos = mul(clip, mInvProjection);
    viewPos.xyz /= viewPos.w;

    float3 dirV = normalize(viewPos.xyz);
    return normalize(mul(dirV, invRot));
}

float4 main(VS_SKYOUT input) : SV_TARGET
{
    float3 camDir = ComputeCamDir(input.Ndc);
    float4 c = _cubemap.Sample(_sp0, camDir);
    return c;
}