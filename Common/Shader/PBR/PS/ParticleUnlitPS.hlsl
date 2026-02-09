#include "../../CommonSharedPS.hlsli"

float4 main(PS_INPUT input) : SV_TARGET
{
    float4 baseColor = _albedo.Sample(_sp0, input.Tex);
    float opacity = _opacity.Sample(_sp0, input.Tex).r;
    float alpha = saturate(baseColor.a * opacity);

    clip(alpha - 0.001f);
    return float4(baseColor.rgb, alpha);
}
