#include "SkyBoxShared.hlsli"

VS_SKYOUT main(uint vertexID : SV_VertexID)
{
    VS_SKYOUT output;

    // 풀스크린 삼각형 하드코딩 (3개의 정점)
    float2 pos;
    if (vertexID == 0)
        pos = float2(-1.0f, -1.0f);
    if (vertexID == 1)
        pos = float2(-1.0f, 3.0f);
    if (vertexID == 2)
        pos = float2(3.0f, -1.0f);

    output.PosH = float4(pos, 0.0f, 1.0f);
    output.PosH.z = output.PosH.w - 0.001f;
    output.Ndc = pos;

    return output;
}
