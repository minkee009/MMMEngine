// Unlit particle pixel shader (tintable)
#include "../CommonSharedPS.hlsli"
#include "../PBR/PBRShared.hlsli"

float4 main(PS_INPUT input) : SV_TARGET
{
    float4 texColor = _albedo.Sample(_sp0, input.Tex);
    float4 baseColor = texColor * mBaseColor;

    if (mUseAlphaClip > 0.5f)
    {
        clip(baseColor.a - mAlphaClip);
    }

    return baseColor;
}
