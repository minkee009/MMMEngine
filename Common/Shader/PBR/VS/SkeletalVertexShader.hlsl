#include "../../CommonSharedVS.hlsli"

VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output = (VS_OUTPUT) 0;

    float4 worldPos = float4(input.Pos, 1.0f);
    
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
        
        skinMat = mul(skinMat, mWorld);
        output.Pos = mul(worldPos, skinMat);
    }
   else {
        output.Pos = mul(worldPos, skinMat);
   }
    
    output.W_Pos = output.Pos;
    output.Pos = mul(output.Pos, mView);
    output.Pos = mul(output.Pos, mProjection);

    float4x4 normalMat = mul(skinMat, mNormalMatrix);
    
    output.Norm = normalize(mul(input.Norm, (float3x3) normalMat));
    output.Tan = normalize(mul(input.Tan, (float3x3) normalMat));
    output.BiTan = normalize(cross(output.Norm, output.Tan));
    output.Tex = input.Tex;
    
    // 현재 위치를 ShadowMap 위치로 변환
    output.S_Pos = mul(float4(output.W_Pos.xyz, 1.0f), mShadowView);
    output.S_Pos = mul(output.S_Pos, mShadowProjection);
    
    return output;
}