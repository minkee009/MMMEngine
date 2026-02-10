#include "../../CommonSharedPS.hlsli"
#include "../PBRShared.hlsli"

float4 main(PS_INPUT input) : SV_TARGET
{
    float4 baseColor = _albedo.Sample(_sp0, input.Tex) * mBaseColor;
    float opacity = _opacity.Sample(_sp0, input.Tex).r;
    float alpha = saturate(baseColor.a * opacity);

    clip(alpha - 0.001f);

    if (mRoundDotClip > 0.5f)
    {
        // 정사각형 세그먼트의 중심에서 원 클리핑
        float2 centeredUv = input.Tex - 0.5f;
        float distSq = dot(centeredUv, centeredUv);
        
        // 반지름 0.5인 정원 클리핑
        clip(0.25f - distSq);
    }

    return float4(baseColor.rgb, alpha);
}
