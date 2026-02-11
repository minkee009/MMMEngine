#include "../../CommonSharedVS.hlsli"

VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output = (VS_OUTPUT) 0;

    float4 localPos = float4(input.Pos, 1.0f);
    
    float4x4 skinMat =
    {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1
    };
    
    if (input.BoneIdx[0] != -1) {
         Matrix tempMat[4] =
        {
            mul(mBoneOffsetMat[input.BoneIdx.x], mBoneMat[input.BoneIdx.x]),
            mul(mBoneOffsetMat[input.BoneIdx.y], mBoneMat[input.BoneIdx.y]),
            mul(mBoneOffsetMat[input.BoneIdx.z], mBoneMat[input.BoneIdx.z]),
            mul(mBoneOffsetMat[input.BoneIdx.w], mBoneMat[input.BoneIdx.w])
        };
        
        skinMat = mul(input.BoneWeight.x, tempMat[0]);
        skinMat += mul(input.BoneWeight.y, tempMat[1]);
        skinMat += mul(input.BoneWeight.z, tempMat[2]);
        skinMat += mul(input.BoneWeight.w, tempMat[3]);
    }
    
    // --- Position: skin -> world ---
    float4 skinnedPos = mul(localPos, skinMat);
    float4 worldPos   = mul(skinnedPos, mWorld);

    output.Pos   = mul(mul(worldPos, mView), mProjection);
    output.W_Pos = worldPos;

    // --- Normal/Tangent: skin(3x3) -> world^-T ---
    float3 n = mul(input.Norm, (float3x3)skinMat);
    float3 t = mul(input.Tan,  (float3x3)skinMat);

    n = normalize(mul(n, (float3x3)mNormalMatrix));
    t = normalize(mul(t, (float3x3)mNormalMatrix));

    output.Norm  = n;
    output.Tan   = t;
    output.BiTan = normalize(cross(output.Norm, output.Tan));
    output.Tex   = input.Tex;

    output.S_Pos = mul(float4(output.W_Pos.xyz, 1.0f), mShadowView);
    output.S_Pos = mul(output.S_Pos, mShadowProjection);

    return output;
}