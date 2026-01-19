Texture2D _albedo : register(t0);
Texture2D _normal: register(t1);
Texture2D _metalic : register(t2);
Texture2D _roughness : register(t3);
Texture2D _ambientOcclusion : register(t4);
Texture2D _emissive : register(t5);
Texture2D _shadowmap : register(t6);
TextureCube _specular : register(t7);
TextureCube _irradiance : register(t8);
Texture2D _brdflut : register(t9);

SamplerState _sp0 : register(s0);

cbuffer Cambuffer : register(b0)
{
    matrix mView;
    matrix mProjection;
    float4 mCamPos;
}

cbuffer Transbuffer : register(b1)
{
    matrix mWorld;
    matrix mNormalMatrix;
}

cbuffer LightBuffer : register(b2)
{
    float4 mLightDir;
    float4 mLightColor;
}

cbuffer MatBuffer : register(b3)
{
    float4 mBaseColor;
    
    float mMetalic;
    float mRoughness;
    float mAoStrength;
    float mEmissive;
}

cbuffer ShadowVP : register(b4)
{
    matrix mShadowView;
    matrix mShadowProjection;
}

cbuffer BoneWorldBuffer : register(b5)
{
    matrix mBoneMat[256];
}

cbuffer BoneOffSetBuffer : register(b6)
{
    matrix mBoneOffsetMat[256];
}

cbuffer ToneBuffer : register(b7)
{
    float mExposure;
    float mBrightness;
    float2 mTonePadding;
}

struct PS_INPUT
{
    float4 Pos : SV_POSITION;
    float4 W_Pos : POSITION;    // 월드 포지션
    float3 Norm : NORMAL;
    float3 Tan : TANGENT;
    float3 BiTan : BITANGENT;
    float2 Tex : TEXCOORD0;     // 텍스쳐 UV
    float4 S_Pos : TEXCOORD1;   // 쉐도우 포지션
};

struct VS_INPUT
{
    float4 Pos : POSITION;
    float3 Norm : NORMAL;
    float3 Tan : TANGENT;
    float3 BiTan : BITANGENT;
    float2 Tex : TEXCOORD0;
    int4 BoneIdx : BONEINDEX;
    float4 BoneWeight : BONEWEIGHT;
};
