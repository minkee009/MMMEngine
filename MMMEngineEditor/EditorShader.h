#pragma once

namespace MMMEngine::EditorShader
{
	inline const char* g_shader_grid = R"(
// Vertex Shader
cbuffer ViewProjBuffer : register(b0)
{
    float4x4 viewProj;
};

struct VS_INPUT
{
    float3 position : POSITION;
};

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float3 worldPosition : TEXCOORD0;
};

PS_INPUT VS_Main(VS_INPUT input)
{
    PS_INPUT output;
    output.worldPosition = input.position;
    output.position = mul(float4(input.position, 1.0f), viewProj);
    return output;
}

// Pixel Shader
cbuffer CameraBuffer : register(b0) 
{
    float3 cameraPosition;
    float padding;
};
float4 PS_Main(PS_INPUT input) : SV_TARGET
{
    float2 uv = input.worldPosition.xz / 1.0; 
    float2 uvDeriv = fwidth(uv);
    float2 scaledThickness = uvDeriv * 1.0; 

    float2 drawPos = abs(frac(uv + 0.5) - 0.5);
    float2 gridAlpha2D = smoothstep(scaledThickness, float2(0.0, 0.0), drawPos);
    
    // X, Z 선 중 더 강한 쪽을 선택
    float gridAlpha = max(gridAlpha2D.x, gridAlpha2D.y);

    // 축(Axis) 선도 동일하게 부드럽게 처리
    float2 axisAlpha2D = smoothstep(uvDeriv * 1.5, float2(0.0, 0.0), abs(input.worldPosition.xz));

    // 거리 기반 페이드
    float distFromCamera = length(input.worldPosition.xz - cameraPosition.xz);
    float fade = 1.0 - smoothstep(4.0, 24.0, distFromCamera);

    // 최종 색상 계산
    float4 color;
    if (axisAlpha2D.y > 0.1)      color = float4(1.0, 0.0, 0.0, axisAlpha2D.y * fade); // X축
    else if (axisAlpha2D.x > 0.1) color = float4(0.0, 0.0, 1.0, axisAlpha2D.x * fade); // Z축
    else                          color = float4(0.5, 0.5, 0.5, gridAlpha * fade);    // 일반 그리드

    // 아주 낮은 알파값은 연산에서 제외하여 성능 최적화
    if (color.a < 0.001) discard;

    return color;
}
)";
}




