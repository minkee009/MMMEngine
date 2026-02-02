cbuffer UIElementBuffer : register(b0)
{
    float4 gRect;
    float4 gUvRect;
    float4 gColor;
    float4 gScreenParams; // x=width, y=height, z=useTexture, w=unused
    float4 gTransformParams0; // x=pivotX, y=pivotY, z=rightX, w=rightY
    float4 gTransformParams1; // x=upX, y=upY, z=unused, w=unused
    float4x4 gViewProj;   // reserved
};

struct VSOut
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};

VSOut main(uint vertexId : SV_VertexID)
{
    float2 quad[6] =
    {
        float2(0.0f, 0.0f),
        float2(1.0f, 0.0f),
        float2(0.0f, 1.0f),
        float2(0.0f, 1.0f),
        float2(1.0f, 0.0f),
        float2(1.0f, 1.0f)
    };

    float2 local = quad[vertexId];
    float2 pivot = gTransformParams0.xy;
    float2 rightDir = gTransformParams0.zw;
    float2 upDir = gTransformParams1.xy;

    float2 localOffset = (local - pivot) * gRect.zw;
    float2 pivotPos = gRect.xy + gRect.zw * pivot;
    float2 localPos2 = pivotPos + rightDir * localOffset.x + upDir * localOffset.y;

    float4 pos;
    float2 ndc;
    ndc.x = (localPos2.x / gScreenParams.x) * 2.0f - 1.0f;
    ndc.y = 1.0f - (localPos2.y / gScreenParams.y) * 2.0f;
    pos = float4(ndc, 0.0f, 1.0f);

    VSOut o;
    o.pos = pos;
    o.uv = lerp(gUvRect.xy, gUvRect.zw, local);
    o.color = gColor;
    return o;
}
