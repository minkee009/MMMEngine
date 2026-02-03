#include "AnimationCurveEditor.h"
#include <algorithm>
#include <cmath>
#include <sstream>

namespace MMMEngine::Editor
{
	namespace
	{
		std::string SerializeCurve(const AnimationCurve& curve)
		{
			std::ostringstream oss;
			oss.precision(6);
			oss << "MMMEngineCurve v1;";
			for (const auto& kf : curve.GetKeyframes())
			{
				oss << kf.time << "," << kf.value << "," << kf.inTangent << "," << kf.outTangent << "," << kf.tangentMode << ";";
			}
			return oss.str();
		}

		bool TryParseCurve(const char* text, AnimationCurve& outCurve)
		{
			if (!text)
				return false;

			const std::string input(text);
			const std::string prefix = "MMMEngineCurve v1;";
			if (input.rfind(prefix, 0) != 0)
				return false;

			std::vector<CurveKeyframe> keys;
			std::string rest = input.substr(prefix.size());
			std::stringstream ss(rest);
			std::string segment;
			while (std::getline(ss, segment, ';'))
			{
				if (segment.empty())
					continue;

				std::stringstream seg(segment);
				std::string token;
				std::vector<std::string> parts;
				while (std::getline(seg, token, ','))
					parts.push_back(token);

				if (parts.size() != 5)
					return false;

				try
				{
					CurveKeyframe kf;
					kf.time = std::stof(parts[0]);
					kf.value = std::stof(parts[1]);
					kf.inTangent = std::stof(parts[2]);
					kf.outTangent = std::stof(parts[3]);
					kf.tangentMode = std::stoi(parts[4]);
					keys.push_back(kf);
				}
				catch (...)
				{
					return false;
				}
			}

			outCurve.SetKeyframes(keys);
			return true;
		}

		/// 키프레임 범위로 뷰 min/max 계산 (autoFit 시)
		AnimationCurveEditorView BuildViewFromCurve(const AnimationCurve& curve, const AnimationCurveEditorView& base)
		{
			AnimationCurveEditorView view = base;
			const auto& keys = curve.GetKeyframes();

			if (!view.autoFit || keys.empty())
				return view;

			float minTime = keys.front().time;
			float maxTime = keys.front().time;
			float minValue = keys.front().value;
			float maxValue = keys.front().value;

			for (const auto& kf : keys)
			{
				minTime = std::min(minTime, kf.time);
				maxTime = std::max(maxTime, kf.time);
				minValue = std::min(minValue, kf.value);
				maxValue = std::max(maxValue, kf.value);
			}

			if (minTime == maxTime)
			{
				minTime -= 0.5f;
				maxTime += 0.5f;
			}
			if (minValue == maxValue)
			{
				minValue -= 0.5f;
				maxValue += 0.5f;
			}

			float padT = (maxTime - minTime) * 0.05f;
			float padV = (maxValue - minValue) * 0.05f;
			if (padT <= 0.0f) padT = 0.5f;
			if (padV <= 0.0f) padV = 0.5f;

			view.min = ImVec2(minTime - padT, minValue - padV);
			view.max = ImVec2(maxTime + padT, maxValue + padV);

			// X/Y 스케일 적용 (비율 고정 꺼진 경우 그리드 보이는 간격 조절. 스케일 줄이면 간격 줄어듦)
			float centerX = (view.min.x + view.max.x) * 0.5f;
			float centerY = (view.min.y + view.max.y) * 0.5f;
			float baseRangeX = view.max.x - view.min.x;
			float baseRangeY = view.max.y - view.min.y;
			float scaleX = std::max(0.1f, view.scaleX);
			float scaleY = std::max(0.1f, view.scaleY);
			float rangeX = baseRangeX / scaleX;
			float rangeY = baseRangeY / scaleY;
			view.min.x = centerX - rangeX * 0.5f;
			view.max.x = centerX + rangeX * 0.5f;
			view.min.y = centerY - rangeY * 0.5f;
			view.max.y = centerY + rangeY * 0.5f;
			return view;
		}

		AnimationCurveEditorView BuildViewFromSegment(const AnimationCurve& curve, const AnimationCurveEditorView& base)
		{
			AnimationCurveEditorView view = base;
			const auto& keys = curve.GetKeyframes();
			if (keys.empty())
				return view;

			float minTime = keys.front().time;
			float maxTime = keys.back().time;
			if (minTime > maxTime)
				std::swap(minTime, maxTime);
			if (minTime == maxTime)
			{
				minTime -= 0.5f;
				maxTime += 0.5f;
			}

			float minValue = keys.front().value;
			float maxValue = keys.front().value;
			const int samples = 64;
			for (int i = 0; i < samples; ++i)
			{
				float t = minTime + (maxTime - minTime) * (static_cast<float>(i) / static_cast<float>(samples - 1));
				float v = curve.Evaluate(t);
				minValue = std::min(minValue, v);
				maxValue = std::max(maxValue, v);
			}
			for (const auto& kf : keys)
			{
				minValue = std::min(minValue, kf.value);
				maxValue = std::max(maxValue, kf.value);
			}

			if (minValue == maxValue)
			{
				minValue -= 0.5f;
				maxValue += 0.5f;
			}

			const float padT = (maxTime - minTime) * 0.02f;
			const float padV = (maxValue - minValue) * 0.02f;
			view.min = ImVec2(minTime - padT, minValue - padV);
			view.max = ImVec2(maxTime + padT, maxValue + padV);
			return view;
		}

		/// (시간, 값) -> 그래프 영역 내 화면 좌표. Y는 아래가 양수이므로 value는 위가 양수.
		ImVec2 ToScreen(const ImRect& rect, const AnimationCurveEditorView& view, float time, float value)
		{
			float w = rect.Max.x - rect.Min.x;
			float h = rect.Max.y - rect.Min.y;
			float tx = (view.max.x - view.min.x) > 0.0f ? (time - view.min.x) / (view.max.x - view.min.x) : 0.0f;
			float ty = (view.max.y - view.min.y) > 0.0f ? (value - view.min.y) / (view.max.y - view.min.y) : 0.0f;
			tx = std::clamp(tx, 0.0f, 1.0f);
			ty = std::clamp(ty, 0.0f, 1.0f);
			return ImVec2(rect.Min.x + tx * w, rect.Max.y - ty * h);
		}

		void DrawBackground(ImDrawList* drawList, const ImRect& rect, float rounding)
		{
			const ImU32 darkGray = IM_COL32(45, 45, 48, 255);
			drawList->AddRectFilled(rect.Min, rect.Max, darkGray, rounding);
			drawList->AddRect(rect.Min, rect.Max, ImGui::GetColorU32(ImGuiCol_Border), rounding);
		}

		float NiceStep(float rough)
		{
			if (rough <= 0.0f) return 1.0f;
			float expv = floorf(log10f(rough));
			float f = rough / powf(10.0f, expv);
			float nice = (f < 1.5f) ? 1.0f : (f < 3.5f) ? 2.0f : (f < 7.5f) ? 5.0f : 10.0f;
			return nice * powf(10.0f, expv);
		}

		void DrawGrid(ImDrawList* drawList, const ImRect& rect, const AnimationCurveEditorView& view, int cols, int rows)
		{
			if (cols <= 0 || rows <= 0) return;
			ImVec4 c = ImGui::GetStyleColorVec4(ImGuiCol_Border);
			c.w *= 0.35f;
			ImU32 grid = ImGui::GetColorU32(c);

			float rangeX = view.max.x - view.min.x;
			float rangeY = view.max.y - view.min.y;
			if (rangeX <= 0.0f || rangeY <= 0.0f) return;

			float stepX = NiceStep(rangeX / static_cast<float>(cols));
			float stepY = NiceStep(rangeY / static_cast<float>(rows));

			float startX = floorf(view.min.x / stepX) * stepX;
			for (float x = startX; x <= view.max.x; x += stepX)
			{
				float tx = (x - view.min.x) / rangeX;
				float sx = rect.Min.x + tx * (rect.Max.x - rect.Min.x);
				drawList->AddLine(ImVec2(sx, rect.Min.y), ImVec2(sx, rect.Max.y), grid);
			}

			float startY = floorf(view.min.y / stepY) * stepY;
			for (float y = startY; y <= view.max.y; y += stepY)
			{
				float ty = (y - view.min.y) / rangeY;
				float sy = rect.Max.y - ty * (rect.Max.y - rect.Min.y);
				drawList->AddLine(ImVec2(rect.Min.x, sy), ImVec2(rect.Max.x, sy), grid);
			}
		}

		void DrawAxisLabels(ImDrawList* drawList, const ImRect& rect, const AnimationCurveEditorView& view)
		{
			const float step = 0.5f;
			float rangeX = view.max.x - view.min.x;
			float rangeY = view.max.y - view.min.y;
			if (rangeX <= 0.0f || rangeY <= 0.0f)
				return;

			ImU32 textCol = ImGui::GetColorU32(ImGuiCol_Text);
			bool hasXAxis = (view.min.y <= 0.0f && view.max.y >= 0.0f);
			bool hasYAxis = (view.min.x <= 0.0f && view.max.x >= 0.0f);

			char buf[32];
			const float eps = 1e-4f;

			if (hasXAxis)
			{
				float t = (0.0f - view.min.y) / rangeY;
				float axisY = rect.Max.y - t * (rect.Max.y - rect.Min.y);
				float startX = floorf(view.min.x / step) * step;
				for (float x = startX; x <= view.max.x + 0.0001f; x += step)
				{
					if (fabsf(x) < eps) continue;
					float tx = (x - view.min.x) / rangeX;
					float sx = rect.Min.x + tx * (rect.Max.x - rect.Min.x);
					snprintf(buf, sizeof(buf), "%.1f", x);
					drawList->AddText(ImVec2(sx + 2.0f, axisY + 2.0f), textCol, buf);
				}
			}

			if (hasYAxis)
			{
				float t = (0.0f - view.min.x) / rangeX;
				float axisX = rect.Min.x + t * (rect.Max.x - rect.Min.x);
				float startY = floorf(view.min.y / step) * step;
				for (float y = startY; y <= view.max.y + 0.0001f; y += step)
				{
					if (fabsf(y) < eps) continue;
					float ty = (y - view.min.y) / rangeY;
					float sy = rect.Max.y - ty * (rect.Max.y - rect.Min.y);
					snprintf(buf, sizeof(buf), "%.1f", y);
					drawList->AddText(ImVec2(axisX + 4.0f, sy - 7.0f), textCol, buf);
				}
			}

			if (hasXAxis && hasYAxis)
			{
				float tx = (0.0f - view.min.x) / rangeX;
				float ty = (0.0f - view.min.y) / rangeY;
				float sx = rect.Min.x + tx * (rect.Max.x - rect.Min.x);
				float sy = rect.Max.y - ty * (rect.Max.y - rect.Min.y);
				drawList->AddText(ImVec2(sx + 4.0f, sy + 2.0f), textCol, "0.0");
			}
		}

		void ApplyAspectLock(AnimationCurveEditorView& view, float width, float height)
		{
			if (!view.lockAspect || width <= 0.0f || height <= 0.0f)
				return;

			float rangeX = view.max.x - view.min.x;
			float rangeY = view.max.y - view.min.y;
			if (rangeX <= 0.0f || rangeY <= 0.0f)
				return;

			const float targetRangeY = rangeX * (height / width);
			const float centerY = (view.min.y + view.max.y) * 0.5f;
			view.min.y = centerY - targetRangeY * 0.5f;
			view.max.y = centerY + targetRangeY * 0.5f;
		}

		void DrawAxes(ImDrawList* drawList, const ImRect& rect, const AnimationCurveEditorView& view)
		{
			ImU32 axis = ImGui::GetColorU32(ImVec4(0.9f, 0.9f, 0.9f, 0.7f));
			float rangeY = view.max.y - view.min.y;
			float rangeX = view.max.x - view.min.x;

			if (rangeY > 0.0f && view.min.y <= 0.0f && view.max.y >= 0.0f)
			{
				float t = (0.0f - view.min.y) / rangeY;
				float y = rect.Max.y - t * (rect.Max.y - rect.Min.y);
				drawList->AddLine(ImVec2(rect.Min.x, y), ImVec2(rect.Max.x, y), axis, 2.0f);
			}
			if (rangeX > 0.0f && view.min.x <= 0.0f && view.max.x >= 0.0f)
			{
				float t = (0.0f - view.min.x) / rangeX;
				float x = rect.Min.x + t * (rect.Max.x - rect.Min.x);
				drawList->AddLine(ImVec2(x, rect.Min.y), ImVec2(x, rect.Max.y), axis, 2.0f);
			}
		}

		void DrawCurveLine(ImDrawList* drawList, const AnimationCurve& curve, const ImRect& rect,
			const AnimationCurveEditorView& view, ImU32 color, float thickness)
		{
			const auto& keys = curve.GetKeyframes();
			if (keys.empty()) return;

			int samples = static_cast<int>(rect.Max.x - rect.Min.x);
			samples = std::clamp(samples, 32, 512);
			float minT = view.min.x;
			float maxT = view.max.x;
			float step = (maxT - minT) / static_cast<float>(samples - 1);

			ImVec2 prev = ToScreen(rect, view, minT, curve.Evaluate(minT));
			for (int i = 1; i < samples; ++i)
			{
				float t = minT + step * static_cast<float>(i);
				ImVec2 curr = ToScreen(rect, view, t, curve.Evaluate(t));
				drawList->AddLine(prev, curr, color, thickness);
				prev = curr;
			}
		}

		void DrawCurveLineRange(ImDrawList* drawList, const AnimationCurve& curve, const ImRect& rect,
			const AnimationCurveEditorView& view, ImU32 color, float thickness, float startT, float endT)
		{
			if (startT >= endT)
				return;

			int samples = static_cast<int>(rect.Max.x - rect.Min.x);
			samples = std::clamp(samples, 32, 512);

			const float minT = view.min.x;
			const float maxT = view.max.x;
			const float drawStart = std::clamp(startT, minT, maxT);
			const float drawEnd = std::clamp(endT, minT, maxT);
			if (drawStart >= drawEnd)
				return;

			const float step = (maxT - minT) / static_cast<float>(samples - 1);
			ImVec2 prev = ToScreen(rect, view, drawStart, curve.Evaluate(drawStart));
			for (int i = 1; i < samples; ++i)
			{
				const float t = minT + step * static_cast<float>(i);
				if (t < drawStart || t > drawEnd)
					continue;
				ImVec2 curr = ToScreen(rect, view, t, curve.Evaluate(t));
				drawList->AddLine(prev, curr, color, thickness);
				prev = curr;
			}
		}

		void DrawKeyframeDots(ImDrawList* drawList, const AnimationCurve& curve, const ImRect& rect,
			const AnimationCurveEditorView& view, ImU32 color, ImU32 edgeColor)
		{
			const float r = 4.0f;
			const auto& keys = curve.GetKeyframes();
			if (keys.empty())
				return;

			const size_t lastIndex = keys.size() - 1;
			for (size_t i = 0; i < keys.size(); ++i)
			{
				const auto& kf = keys[i];
				ImU32 col = (i == 0 || i == lastIndex) ? edgeColor : color;
				ImVec2 p = ToScreen(rect, view, kf.time, kf.value);
				ImVec2 a(p.x, p.y - r);
				ImVec2 b(p.x + r, p.y);
				ImVec2 c(p.x, p.y + r);
				ImVec2 d(p.x - r, p.y);
				drawList->AddQuadFilled(a, b, c, d, col);
			}
		}
	}

	bool DrawAnimationCurvePreview(AnimationCurve& curve, const ImVec2& size, const AnimationCurveEditorView& view, bool* outChanged)
	{
		if (outChanged) *outChanged = false;
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		ImVec2 cursor = ImGui::GetCursorScreenPos();
		ImRect rect(cursor, ImVec2(cursor.x + size.x, cursor.y + size.y));

		AnimationCurveEditorView fitted = BuildViewFromSegment(curve, view);

		DrawBackground(drawList, rect, 4.0f);
		drawList->PushClipRect(rect.Min, rect.Max, true);
		const ImU32 curveColor = IM_COL32(120, 200, 255, 255);
		{
			const auto& keys = curve.GetKeyframes();
			if (!keys.empty())
			{
				const ImU32 yellow = IM_COL32(255, 220, 80, 255);
				DrawCurveLineRange(drawList, curve, rect, fitted, yellow, 2.5f, keys.front().time, keys.back().time);
			}
			else
			{
				DrawCurveLine(drawList, curve, rect, fitted, curveColor, 1.5f);
			}
		}
		DrawKeyframeDots(drawList, curve, rect, fitted, ImGui::GetColorU32(ImGuiCol_PlotHistogram), IM_COL32(255, 220, 80, 255));
		drawList->PopClipRect();

		if (curve.IsEmpty())
		{
			ImGui::SetCursorScreenPos(ImVec2(cursor.x + 6.0f, cursor.y + 6.0f));
			ImGui::TextUnformatted("Empty");
		}

		ImGui::SetCursorScreenPos(cursor);
		ImGui::InvisibleButton("##curve_preview", size);

		bool clicked = ImGui::IsItemClicked();

		static bool s_openInvalidPopup = false;
		if (ImGui::BeginPopupContextItem("CurvePreviewContext"))
		{
			if (ImGui::MenuItem(u8"복사"))
			{
				std::string serialized = SerializeCurve(curve);
				ImGui::SetClipboardText(serialized.c_str());
			}
			if (ImGui::MenuItem(u8"붙여넣기"))
			{
				const char* clip = ImGui::GetClipboardText();
				AnimationCurve parsed;
				if (TryParseCurve(clip, parsed))
				{
					curve = parsed;
					if (outChanged) *outChanged = true;
				}
				else
				{
					s_openInvalidPopup = true;
				}
			}
			ImGui::EndPopup();
		}

		if (s_openInvalidPopup)
		{
			ImGui::OpenPopup("CurveClipboardInvalid");
			s_openInvalidPopup = false;
		}
		if (ImGui::BeginPopupModal("CurveClipboardInvalid", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::TextUnformatted(u8"인식할 수 없는 데이터입니다.");
			if (ImGui::Button(u8"확인"))
				ImGui::CloseCurrentPopup();
			ImGui::EndPopup();
		}

		return clicked;
	}

	void DrawAnimationCurveGraph(AnimationCurve& curve, const ImVec2& size, AnimationCurveEditorView& view)
	{
		view = BuildViewFromCurve(curve, view);

		ImDrawList* drawList = ImGui::GetWindowDrawList();
		ImVec2 cursor = ImGui::GetCursorScreenPos();
		ImRect rect(cursor, ImVec2(cursor.x + size.x, cursor.y + size.y));

		ImGuiIO& io = ImGui::GetIO();
		ImVec2 mouse = io.MousePos;
		bool hoveredRect = rect.Contains(mouse);
		float w = rect.Max.x - rect.Min.x;
		float h = rect.Max.y - rect.Min.y;

		// 비율 고정이 아닐 때: 창 크기가 바뀌면 설정한 데이터 비율(스케일) 유지 — 범위를 픽셀 비율에 맞춰 조정
		if (!view.lockAspect && view.lastGraphWidth > 0.0f && view.lastGraphHeight > 0.0f && w > 0.0f && h > 0.0f)
		{
			float rangeX = view.max.x - view.min.x;
			float rangeY = view.max.y - view.min.y;
			if (rangeX > 0.0f && rangeY > 0.0f)
			{
				float centerX = (view.min.x + view.max.x) * 0.5f;
				float centerY = (view.min.y + view.max.y) * 0.5f;
				float newRangeX = rangeX * (w / view.lastGraphWidth);
				float newRangeY = rangeY * (h / view.lastGraphHeight);
				view.min.x = centerX - newRangeX * 0.5f;
				view.max.x = centerX + newRangeX * 0.5f;
				view.min.y = centerY - newRangeY * 0.5f;
				view.max.y = centerY + newRangeY * 0.5f;
			}
		}
		view.lastGraphWidth = w;
		view.lastGraphHeight = h;

		float timePerPx = 0.0f;
		float valuePerPx = 0.0f;
		auto RecalcPerPx = [&]()
		{
			timePerPx = (w > 0.0f) ? (view.max.x - view.min.x) / w : 0.0f;
			valuePerPx = (h > 0.0f) ? (view.max.y - view.min.y) / h : 0.0f;
		};

		ApplyAspectLock(view, w, h);
		RecalcPerPx();

		auto FromScreen = [&rect, &view](float px, float py)
		{
			float w = rect.Max.x - rect.Min.x;
			float h = rect.Max.y - rect.Min.y;
			if (w <= 0.0f || h <= 0.0f) return ImVec2(view.min.x, view.min.y);
			float tx = (px - rect.Min.x) / w;
			float ty = 1.0f - (py - rect.Min.y) / h;
			float time = view.min.x + tx * (view.max.x - view.min.x);
			float value = view.min.y + ty * (view.max.y - view.min.y);
			return ImVec2(time, value);
		};

		const float handleLen = 40.0f; // 화면상 고정 길이
		auto HandlePos = [&](const ImVec2& keyScreenPos, float slope, bool outDir)
		{
			float rangeX = view.max.x - view.min.x;
			float rangeY = view.max.y - view.min.y;
			if (rangeX <= 0.0f || rangeY <= 0.0f)
				return keyScreenPos;

			float dx_screen = (1.0f / rangeX) * w;
			float dy_screen = (-slope / rangeY) * h;
			ImVec2 dir(dx_screen, dy_screen);
			float len = sqrtf(dir.x * dir.x + dir.y * dir.y);
			if (len < 1e-4f)
			{
				dir = ImVec2(1.0f, 0.0f);
				len = 1.0f;
			}
			dir.x /= len;
			dir.y /= len;
			float s = outDir ? 1.0f : -1.0f;
			return ImVec2(keyScreenPos.x + dir.x * handleLen * s, keyScreenPos.y + dir.y * handleLen * s);
		};
		// 중클릭 드래그: 뷰 이동 (그리드도 같이 이동)
		if (hoveredRect && ImGui::IsMouseDown(ImGuiMouseButton_Middle))
		{
			view.autoFit = false;
			ImVec2 d = io.MouseDelta;
			view.min.x -= d.x * timePerPx;
			view.max.x -= d.x * timePerPx;
			view.min.y += d.y * valuePerPx;
			view.max.y += d.y * valuePerPx;
			ApplyAspectLock(view, w, h);
			RecalcPerPx();
		}

		// 휠 줌: 마우스 위치 기준
		if (hoveredRect && io.MouseWheel != 0.0f)
		{
			view.autoFit = false;
			ImVec2 tv = FromScreen(mouse.x, mouse.y);
			float rangeX = view.max.x - view.min.x;
			float rangeY = view.max.y - view.min.y;
			if (rangeX > 0.0f && rangeY > 0.0f)
			{
				float t = (tv.x - view.min.x) / rangeX;
				float v = (tv.y - view.min.y) / rangeY;
				float zoom = powf(1.1f, -io.MouseWheel);
				float newRangeX = rangeX * zoom;
				float newRangeY = rangeY * zoom;
				view.min.x = tv.x - t * newRangeX;
				view.max.x = view.min.x + newRangeX;
				view.min.y = tv.y - v * newRangeY;
				view.max.y = view.min.y + newRangeY;
				ApplyAspectLock(view, w, h);
				RecalcPerPx();
			}
		}

		DrawBackground(drawList, rect, 6.0f);
		drawList->PushClipRect(rect.Min, rect.Max, true);
		{
			const float gridPx = 60.0f;
			int cols = std::max(2, static_cast<int>(std::round(w / gridPx)));
			int rows = std::max(2, static_cast<int>(std::round(h / gridPx)));
			DrawGrid(drawList, rect, view, cols, rows);
		}
		DrawAxes(drawList, rect, view);
		const ImU32 curveColor = IM_COL32(120, 200, 255, 255);
		DrawCurveLine(drawList, curve, rect, view, curveColor, 2.0f);
		{
			const auto& keys = curve.GetKeyframes();
			if (!keys.empty())
			{
				const ImU32 yellow = IM_COL32(255, 220, 80, 255);
				DrawCurveLineRange(drawList, curve, rect, view, yellow, 2.5f, keys.front().time, keys.back().time);
			}
		}
		DrawAxisLabels(drawList, rect, view);
		DrawKeyframeDots(drawList, curve, rect, view, ImGui::GetColorU32(ImGuiCol_PlotHistogram), IM_COL32(255, 220, 80, 255));
		drawList->PopClipRect();

		if (curve.IsEmpty())
		{
			ImGui::SetCursorScreenPos(ImVec2(cursor.x + 8.0f, cursor.y + 8.0f));
			ImGui::TextUnformatted("No keyframes");
		}

		ImGui::SetCursorScreenPos(cursor);
		ImGui::InvisibleButton("##curve_graph", size);

		ImGui::PushID(&curve);

		bool hovered = ImGui::IsItemHovered();

		const float hitRadius = 8.0f;
		const float handleRadius = 6.0f;

		static int s_selectedKeyIndex = -1;
		static bool s_draggingPoint = false;
		static int s_dragHandle = -1; // 0=in, 1=out
		static float s_dragOffsetTime = 0.0f, s_dragOffsetValue = 0.0f;

		static int s_contextKeyIndex = -1;
		static int s_editKeyIndex = -1;
		static bool s_openEditPopup = false;
		static float s_addTime = 0.0f, s_addValue = 0.0f;

		auto HitTestKey = [&](ImVec2 pos, float radius)
		{
			const auto& keys = curve.GetKeyframes();
			for (size_t i = 0; i < keys.size(); ++i)
			{
				ImVec2 p = ToScreen(rect, view, keys[i].time, keys[i].value);
				float dx = pos.x - p.x, dy = pos.y - p.y;
				if (dx * dx + dy * dy <= radius * radius)
					return static_cast<int>(i);
			}
			return -1;
		};

		auto FindKeyIndex = [&](const std::vector<CurveKeyframe>& keys, const CurveKeyframe& target)
		{
			const float eps = 1e-3f;
			for (size_t i = 0; i < keys.size(); ++i)
			{
				if (fabsf(keys[i].time - target.time) < eps && fabsf(keys[i].value - target.value) < eps)
					return static_cast<int>(i);
			}
			return -1;
		};

		// 선택된 키에 대한 탄젠트 핸들 계산
		bool hasSelected = false;
		ImVec2 handleIn(0, 0), handleOut(0, 0), keyScreen(0, 0);
		{
			const auto& keys = curve.GetKeyframes();
			if (s_selectedKeyIndex >= 0 && s_selectedKeyIndex < static_cast<int>(keys.size()))
			{
				hasSelected = true;
				const CurveKeyframe& kf = keys[s_selectedKeyIndex];
				keyScreen = ToScreen(rect, view, kf.time, kf.value);
				handleIn = HandlePos(keyScreen, kf.inTangent, false);
				handleOut = HandlePos(keyScreen, kf.outTangent, true);
			}
		}

		// 좌클릭: 선택/드래그 시작 (핸들 우선)
		if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
		{
			if (hasSelected)
			{
				float dxIn = mouse.x - handleIn.x, dyIn = mouse.y - handleIn.y;
				float dxOut = mouse.x - handleOut.x, dyOut = mouse.y - handleOut.y;
				if (dxIn * dxIn + dyIn * dyIn <= handleRadius * handleRadius)
				{
					view.autoFit = false;
					s_dragHandle = 0;
					s_draggingPoint = false;
				}
				else if (dxOut * dxOut + dyOut * dyOut <= handleRadius * handleRadius)
				{
					view.autoFit = false;
					s_dragHandle = 1;
					s_draggingPoint = false;
				}
				else
				{
					s_dragHandle = -1;
				}
			}

			if (s_dragHandle == -1)
			{
				int hit = HitTestKey(mouse, hitRadius);
				if (hit >= 0)
				{
					view.autoFit = false;
					s_selectedKeyIndex = hit;
					s_draggingPoint = true;
					ImVec2 tv = FromScreen(mouse.x, mouse.y);
					const auto& keys = curve.GetKeyframes();
					s_dragOffsetTime = keys[hit].time - tv.x;
					s_dragOffsetValue = keys[hit].value - tv.y;
				}
				else
				{
					s_selectedKeyIndex = -1;
					s_draggingPoint = false;
				}
			}
		}

		// 점 드래그
		if (s_draggingPoint && ImGui::IsMouseDown(ImGuiMouseButton_Left))
		{
			std::vector<CurveKeyframe> keyframes = curve.GetKeyframes();
			if (s_selectedKeyIndex >= 0 && s_selectedKeyIndex < static_cast<int>(keyframes.size()))
			{
				ImVec2 tv = FromScreen(mouse.x, mouse.y);
				CurveKeyframe kf = keyframes[s_selectedKeyIndex];
				kf.time = tv.x + s_dragOffsetTime;
				kf.value = tv.y + s_dragOffsetValue;
				keyframes[s_selectedKeyIndex] = kf;
				curve.SetKeyframes(keyframes);
				int newIndex = FindKeyIndex(curve.GetKeyframes(), kf);
				if (newIndex >= 0) s_selectedKeyIndex = newIndex;
			}
		}
		else if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
		{
			s_draggingPoint = false;
		}

		// 탄젠트 핸들 드래그
		if (s_dragHandle != -1 && ImGui::IsMouseDown(ImGuiMouseButton_Left))
		{
			std::vector<CurveKeyframe> keyframes = curve.GetKeyframes();
			if (s_selectedKeyIndex >= 0 && s_selectedKeyIndex < static_cast<int>(keyframes.size()))
			{
				CurveKeyframe kf = keyframes[s_selectedKeyIndex];
				ImVec2 tv = FromScreen(mouse.x, mouse.y);
				float dx = tv.x - kf.time;
				if (fabsf(dx) > 1e-5f)
				{
					float slope = (tv.y - kf.value) / dx;
					if (kf.tangentMode == 1)
					{
						kf.inTangent = slope;
						kf.outTangent = slope;
					}
					else
					{
						if (s_dragHandle == 0) kf.inTangent = slope;
						else kf.outTangent = slope;
					}
					keyframes[s_selectedKeyIndex] = kf;
					curve.SetKeyframes(keyframes);
				}
			}
		}
		else if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
		{
			s_dragHandle = -1;
		}

		// 우클릭: 빈곳 -> 점 생성, 점 위 -> 점 삭제/값·시간 변경/탄젠트 락
		if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
		{
			int hit = HitTestKey(mouse, hitRadius);
			if (hit >= 0)
			{
				view.autoFit = false;
				s_contextKeyIndex = hit;
				s_selectedKeyIndex = hit;
				ImGui::OpenPopup("CurveKeyContext");
			}
			else
			{
				view.autoFit = false;
				ImVec2 tv = FromScreen(mouse.x, mouse.y);
				s_addTime = tv.x; s_addValue = tv.y;
				ImGui::OpenPopup("CurveEmptyContext");
			}
		}

		if (ImGui::BeginPopup("CurveEmptyContext"))
		{
			if (ImGui::MenuItem(u8"점 생성"))
			{
				view.autoFit = false;
				curve.AddKeyframe(s_addTime, s_addValue, 0.0f, 0.0f, 1);
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}

		if (ImGui::BeginPopup("CurveKeyContext"))
		{
			std::vector<CurveKeyframe> keyframes = curve.GetKeyframes();
			if (s_contextKeyIndex >= 0 && s_contextKeyIndex < static_cast<int>(keyframes.size()))
			{
				CurveKeyframe& kf = keyframes[s_contextKeyIndex];
				if (ImGui::MenuItem(u8"점 삭제"))
				{
					view.autoFit = false;
					if (keyframes.size() <= 1)
					{
						curve.Clear();
					}
					else
					{
						keyframes.erase(keyframes.begin() + s_contextKeyIndex);
						curve.SetKeyframes(keyframes);
					}
					s_selectedKeyIndex = -1;
					ImGui::CloseCurrentPopup();
				}
				if (ImGui::MenuItem(u8"값 변경") || ImGui::MenuItem(u8"시간 변경"))
				{
					view.autoFit = false;
					s_editKeyIndex = s_contextKeyIndex;
					s_openEditPopup = true;
					ImGui::CloseCurrentPopup();
				}

				bool lock = (kf.tangentMode == 1);
				if (ImGui::Checkbox(u8"탄젠트 락", &lock))
				{
					view.autoFit = false;
					kf.tangentMode = lock ? 1 : 0;
					if (lock) kf.outTangent = kf.inTangent;
					keyframes[s_contextKeyIndex] = kf;
					curve.SetKeyframes(keyframes);
				}
			}
			ImGui::EndPopup();
		}

		if (s_openEditPopup)
		{
			ImGui::OpenPopup("EditKeyframe");
			s_openEditPopup = false;
		}

		if (ImGui::BeginPopup("EditKeyframe"))
		{
			std::vector<CurveKeyframe> keyframes = curve.GetKeyframes();
			if (s_editKeyIndex >= 0 && s_editKeyIndex < static_cast<int>(keyframes.size()))
			{
				static int s_lastEditIndex = -1;
				static bool s_editTimeInput = false;
				static bool s_editValueInput = false;
				static bool s_editInTanInput = false;
				static bool s_editOutTanInput = false;
				static float s_editTime = 0.0f;
				static float s_editValue = 0.0f;
				static float s_editInTan = 0.0f;
				static float s_editOutTan = 0.0f;
				static int s_editTangentMode = 1;

				if (s_lastEditIndex != s_editKeyIndex)
				{
					s_lastEditIndex = s_editKeyIndex;
					s_editTimeInput = false;
					s_editValueInput = false;
					s_editInTanInput = false;
					s_editOutTanInput = false;
					s_editTime = keyframes[s_editKeyIndex].time;
					s_editValue = keyframes[s_editKeyIndex].value;
					s_editInTan = keyframes[s_editKeyIndex].inTangent;
					s_editOutTan = keyframes[s_editKeyIndex].outTangent;
					s_editTangentMode = keyframes[s_editKeyIndex].tangentMode;
				}
				else
				{
					const CurveKeyframe& current = keyframes[s_editKeyIndex];
					if (!s_editTimeInput) s_editTime = current.time;
					if (!s_editValueInput) s_editValue = current.value;
					if (!s_editInTanInput) s_editInTan = current.inTangent;
					if (!s_editOutTanInput) s_editOutTan = current.outTangent;
					s_editTangentMode = current.tangentMode;
				}

				bool changed = false;

				ImGui::SetNextItemWidth(120.0f);
				if (s_editTimeInput)
				{
					if (ImGui::InputFloat(u8"시간", &s_editTime, 0.0f, 0.0f, "%.3f", ImGuiInputTextFlags_EnterReturnsTrue))
						changed = true;
					if (ImGui::IsItemDeactivatedAfterEdit())
						s_editTimeInput = false;
				}
				else
				{
					if (ImGui::DragFloat(u8"시간", &s_editTime, 0.01f))
						changed = true;
					if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
						s_editTimeInput = true;
				}

				ImGui::SetNextItemWidth(120.0f);
				if (s_editValueInput)
				{
					if (ImGui::InputFloat(u8"값", &s_editValue, 0.0f, 0.0f, "%.3f", ImGuiInputTextFlags_EnterReturnsTrue))
						changed = true;
					if (ImGui::IsItemDeactivatedAfterEdit())
						s_editValueInput = false;
				}
				else
				{
					if (ImGui::DragFloat(u8"값", &s_editValue, 0.01f))
						changed = true;
					if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
						s_editValueInput = true;
				}

				const bool tangentLocked = (s_editTangentMode == 1);
				if (tangentLocked)
				{
					ImGui::SetNextItemWidth(120.0f);
					if (s_editInTanInput)
					{
						if (ImGui::InputFloat(u8"탄젠트", &s_editInTan, 0.0f, 0.0f, "%.3f", ImGuiInputTextFlags_EnterReturnsTrue))
							changed = true;
						if (ImGui::IsItemDeactivatedAfterEdit())
							s_editInTanInput = false;
					}
					else
					{
						if (ImGui::DragFloat(u8"탄젠트", &s_editInTan, 0.01f))
							changed = true;
						if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
							s_editInTanInput = true;
					}
					s_editOutTan = s_editInTan;
				}
				else
				{
					ImGui::SetNextItemWidth(120.0f);
					if (s_editInTanInput)
					{
						if (ImGui::InputFloat(u8"인 탄젠트", &s_editInTan, 0.0f, 0.0f, "%.3f", ImGuiInputTextFlags_EnterReturnsTrue))
							changed = true;
						if (ImGui::IsItemDeactivatedAfterEdit())
							s_editInTanInput = false;
					}
					else
					{
						if (ImGui::DragFloat(u8"인 탄젠트", &s_editInTan, 0.01f))
							changed = true;
						if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
							s_editInTanInput = true;
					}

					ImGui::SetNextItemWidth(120.0f);
					if (s_editOutTanInput)
					{
						if (ImGui::InputFloat(u8"아웃 탄젠트", &s_editOutTan, 0.0f, 0.0f, "%.3f", ImGuiInputTextFlags_EnterReturnsTrue))
							changed = true;
						if (ImGui::IsItemDeactivatedAfterEdit())
							s_editOutTanInput = false;
					}
					else
					{
						if (ImGui::DragFloat(u8"아웃 탄젠트", &s_editOutTan, 0.01f))
							changed = true;
						if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
							s_editOutTanInput = true;
					}
				}

				if (changed)
				{
					view.autoFit = false;
					CurveKeyframe kf = keyframes[s_editKeyIndex];
					kf.time = s_editTime;
					kf.value = s_editValue;
					kf.inTangent = s_editInTan;
					kf.outTangent = s_editOutTan;
					kf.tangentMode = s_editTangentMode;
					keyframes[s_editKeyIndex] = kf;
					curve.SetKeyframes(keyframes);
					int newIndex = FindKeyIndex(curve.GetKeyframes(), kf);
					if (newIndex >= 0) s_selectedKeyIndex = newIndex;
				}
			}
			else
				ImGui::CloseCurrentPopup();
			ImGui::EndPopup();
		}

		// 선택 강조 및 탄젠트 핸들 표시
		{
			const auto& keys = curve.GetKeyframes();
			if (s_selectedKeyIndex >= 0 && s_selectedKeyIndex < static_cast<int>(keys.size()))
			{
				const CurveKeyframe& kf = keys[s_selectedKeyIndex];
				ImVec2 keyPos = ToScreen(rect, view, kf.time, kf.value);
				ImVec2 hIn = HandlePos(keyPos, kf.inTangent, false);
				ImVec2 hOut = HandlePos(keyPos, kf.outTangent, true);

				drawList->PushClipRect(rect.Min, rect.Max, true);
				ImU32 lineCol = ImGui::GetColorU32(ImVec4(0.9f, 0.9f, 0.9f, 0.7f));
				ImU32 handleCol = ImGui::GetColorU32(ImVec4(0.9f, 0.9f, 0.9f, 1.0f));
				drawList->AddLine(keyPos, hIn, lineCol, 1.0f);
				drawList->AddLine(keyPos, hOut, lineCol, 1.0f);
				drawList->AddCircleFilled(hIn, handleRadius, handleCol);
				drawList->AddCircleFilled(hOut, handleRadius, handleCol);
				drawList->AddCircleFilled(keyPos, 4.5f, handleCol);
				drawList->PopClipRect();
			}
		}

		ImGui::PopID();
	}
}
