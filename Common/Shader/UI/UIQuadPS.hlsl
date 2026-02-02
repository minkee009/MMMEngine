Texture2D gTex : register(t0);
SamplerState gSampler : register(s0);

cbuffer UIElementBuffer : register(b0)
{
    float4 gRect;
    float4 gUvRect;
    float4 gColor;
    float4 gScreenParams; // z=useTexture
};

struct PSIn
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};

float4 main(PSIn input) : SV_TARGET
{
    float useTex = gScreenParams.z;
    float4 texColor = (useTex > 0.5f) ? gTex.Sample(gSampler, input.uv) : float4(1.0f, 1.0f, 1.0f, 1.0f);
    return texColor * input.color;
}
