#pragma once
#include "RenderQueue.h"
#include <DirectXMath.h>

namespace MMMEngine
{
    struct RenderCommand
    {
        // [1] 정렬을 위한 키 데이터
        RenderQueue  queue;             // 1순위: 렌더 패스 (Opaque, Transparent 등)
        int          renderPriority;    // 2순위: 사용자 정의 순서 (기본값 0)
        float        distanceSq;        // 3순위: 카메라와의 거리 제곱 (투명도 정렬용)
        uint32_t     materialID;        // 4순위: 상태 변화 최소화 (같은 머터리얼끼리 모으기)

        // [2] 실제 드로잉 리소스 (Raw 포인터)
        class Mesh* mesh;
        class Material* material;

        // [3] 변환 및 상태 데이터
        DirectX::XMMATRIX worldMatrix;

        // [4] 스키닝 정보 (질문하신 핵심 로직)
        bool   isSkinned;
        const DirectX::XMMATRIX* boneMatrices; // 애니메이션 포즈 데이터 포인터
        uint32_t boneCount;

        // [5] 기타 필드
        uint32_t layer; // 카메라 컬링 마스크 비교용
    };
}