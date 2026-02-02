TextureCube _cubemap : register(t0);
SamplerState _sp0 : register(s0);

cbuffer CamBuffer : register(b0)
{
    matrix mView;
    matrix mProjection;
    float4 mCamPos;
    matrix mInvProjection;
}

struct VS_SKYOUT
{ 
    float4 PosH : SV_POSITION;
    float2 Ndc : TEXCOORD0;
};