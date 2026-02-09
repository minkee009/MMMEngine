#include "../../CommonSharedPS.hlsli"

cbuffer ParticleBuffer : register(b10)
{
    float mParticleAlpha;
    float3 _particlePadding;
}

float4 main(PS_INPUT input) : SV_TARGET
{
    float4 baseColor = _albedo.Sample(_sp0, input.Tex);
    float opacity = _opacity.Sample(_sp0, input.Tex).r;
    float baseAlpha = saturate(baseColor.a * opacity);
    float instanceAlpha = (input.ParticleAlpha > 0.0f) ? saturate(input.ParticleAlpha) : 1.0f;
    float alpha = saturate(baseAlpha * mParticleAlpha * instanceAlpha);

    // Keep cutout stable and apply fade on top to avoid per-pixel popping.
    clip(baseAlpha - 0.001f);
    return float4(baseColor.rgb, alpha);
}
