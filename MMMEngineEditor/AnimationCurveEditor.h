#pragma once

#include "imgui.h"
#include "imgui_internal.h"
#include "AnimationCurve.h"

namespace MMMEngine::Editor
{
	/// 그래프 뷰포트: X = 시간, Y = 값
	struct AnimationCurveEditorView
	{
		ImVec2 min = ImVec2(0.0f, 0.0f);
		ImVec2 max = ImVec2(1.0f, 1.0f);
		bool autoFit = true;
		bool lockAspect = false;
		/// X/Y축 뷰 스케일 (그리드 보이는 간격). 비율 고정 꺼진 경우에만 사용. 1 = 기본, >1 = 더 넓은 범위.
		float scaleX = 1.0f;
		float scaleY = 1.0f;
		/// 이전 그래프 픽셀 크기 (창 조절 시 비율 유지용, 0이면 미사용)
		float lastGraphWidth = 0.0f;
		float lastGraphHeight = 0.0f;
	};

	/// 인스펙터용 미리보기. 클릭 시 true 반환.
	bool DrawAnimationCurvePreview(AnimationCurve& curve, const ImVec2& size, const AnimationCurveEditorView& view, bool* outChanged = nullptr);

	/// 커브 편집기 창 내부: 시간(X) vs 값(Y) 2D 그래프만 그림. view는 표시 범위(수정됨).
	void DrawAnimationCurveGraph(AnimationCurve& curve, const ImVec2& size, AnimationCurveEditorView& view);
}
