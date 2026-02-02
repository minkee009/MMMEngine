Texture2D    _Albedo    : register(t0);   // 알베도 텍스처
SamplerState _Sampler   : register(s0);

struct PSInput
{
    float4 pos      : SV_POSITION;   // 라이트 뷰-프로젝션 좌표
    float2 texcoord : TEXCOORD0;     // UV 좌표
};

float4 main(PSInput input) : SV_TARGET
{
    // 알베도 텍스처 샘플링
    float alpha = _Albedo.Sample(_Sampler, input.texcoord).a;

    // 알파 테스트 (cutout)
    // threshold는 필요에 따라 0.5f 등으로 조정
    clip(alpha - 0.5f);

    // 쉐도우맵은 깊이만 기록하므로 색상 출력은 필요 없음
    // SV_DEPTH를 쓰는 경우에는 float depth 반환
    // 여기서는 단순히 0 반환 (RTV가 없고 DSV에만 기록됨)
    return float4(0,0,0,0);
}
