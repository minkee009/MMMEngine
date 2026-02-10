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
        float2 centeredUv = input.Tex * 2.0f - 1.0f;
        float circleMask = 1.0f - dot(centeredUv, centeredUv);
        clip(circleMask);
    }

    return float4(baseColor.rgb, alpha);
}

