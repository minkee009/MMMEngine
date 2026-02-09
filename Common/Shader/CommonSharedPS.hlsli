Texture2D _albedo : register(t0);
Texture2D _normal: register(t1);
Texture2D _emissive : register(t2);
Texture2D _shadowmap : register(t3);
Texture2D _opacity : register(t4);

SamplerState _sp0 : register(s0);
SamplerComparisonState _cmpsp0 : register(s1);
SamplerState _samPoint : register(s2);

cbuffer Cambuffer : register(b0)
{
    matrix mView;
    matrix mProjection;
    float4 mCamPos;
    matrix mInvProjection;
}

cbuffer LightBuffer : register(b1)
{
    float3 mLightDir;
    float  mLightPadding;

    float3 mLightColor;
    float  mIntensity;

    float4 mLightPos;
}

struct PS_INPUT
{
    float4 Pos : SV_POSITION;
    float3 Norm : NORMAL;
    float3 Tan : TANGENT;
    float3 BiTan : BITANGENT;
    float2 Tex : TEXCOORD0;
    float4 S_Pos : TEXCOORD1;
    float4 W_Pos : TEXCOORD2;
    float ParticleAlpha : TEXCOORD3;
};
