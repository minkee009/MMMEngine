#pragma once

namespace MMMEngine::EditorShader
{
    inline const char* g_shader_grid = R"(
// Vertex Shader Constant Buffer
cbuffer ViewProjBuffer : register(b0)
{
    float4x4 viewProj;
};

struct VS_INPUT { float3 position : POSITION; };
struct PS_INPUT { float4 position : SV_POSITION; float3 worldPosition : TEXCOORD0; };

PS_INPUT VS_Main(VS_INPUT input)
{
    PS_INPUT output;
    output.worldPosition = input.position;
    output.position = mul(float4(input.position, 1.0f), viewProj);
    return output;
}

// Pixel Shader Constant Buffer (C++의 PSSetConstantBuffers(0, ...)에 맞춤)
cbuffer CameraBuffer : register(b1)
{
    float3 cameraPosition;
    float cb_padding;
};

// 'line' 대신 'gridVal' 변수명 사용 (예약어 충돌 방지)
float drawGrid(float2 uv, float thickness)
{
    float2 derivative = fwidth(uv);
    derivative = max(derivative, float2(0.0001, 0.0001)); // 0 나누기 방지
    
    float2 gridPos = abs(frac(uv - 0.5) - 0.5) / derivative;
    float gridVal = min(gridPos.x, gridPos.y);
    
    return 1.0 - smoothstep(0.0, thickness, gridVal);
}

float4 PS_Main(PS_INPUT input) : SV_TARGET
{
    float2 uv = input.worldPosition.xz;
    float2 derivative = fwidth(uv);
    derivative = max(derivative, float2(0.0001, 0.0001));

    // 1. 거리 기반 페이드
    float dist = length(input.worldPosition - cameraPosition);
    float fade = 1.0 - smoothstep(15.0, 45.0, dist); 
    
    // 2. 축(Axis) 렌더링 - 픽셀 단위 두께 고정
    float2 axisPos = abs(input.worldPosition.xz) / derivative;
    float4 finalColor = float4(0.0, 0.0, 0.0, 0.0);
    
    float axisWidth = 1.0;

    // 축 선 판정 로직
    if (axisPos.x < axisWidth) {
        finalColor = float4(0.2, 0.4, 1.0, fade); // Z축 (Blue)
    }
    else if (axisPos.y < axisWidth) {
        finalColor = float4(1.0, 0.3, 0.3, fade); // X축 (Red)
    }
    else {
        // 일반 그리드 (1단위 & 10단위)
        float g1 = drawGrid(uv, 1.0);
        float g2 = drawGrid(uv * 0.1, 1.0);
        
        float alpha = max(g1 * 0.15, g2 * 0.4);
        finalColor = float4(0.5, 0.5, 0.5, alpha * fade);
    }

    if (finalColor.a < 0.01) discard;
    return finalColor;
}
)";
}