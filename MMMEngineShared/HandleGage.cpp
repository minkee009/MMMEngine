#include "HandleGage.h"
#include "RectTransform.h"
#include "Canvas.h"
#include "RenderManager.h"
#include "Texture2D.h"
#include "rttr/registration"

RTTR_REGISTRATION
{
	using namespace rttr;
	using namespace MMMEngine;

	registration::class_<HandleGage>("HandleGage")
		(rttr::metadata("wrapper_type_name", "ObjPtr<HandleGage>"))
		.property("BackgroundOffset", &HandleGage::GetBackgroundOffset, &HandleGage::SetBackgroundOffset)
		.property("BackgroundScale", &HandleGage::GetBackgroundScale, &HandleGage::SetBackgroundScale)
		.property("FillOffset", &HandleGage::GetFillOffset, &HandleGage::SetFillOffset)
		.property("FillScale", &HandleGage::GetFillScale, &HandleGage::SetFillScale)
		.property("Color", &HandleGage::GetColor, &HandleGage::SetColor)
		.property("BackgroundTexture", &HandleGage::GetBackgroundTexture, &HandleGage::SetBackgroundTexture)
		.property("FillTexture", &HandleGage::GetFillTexture, &HandleGage::SetFillTexture)
		.property("HandleTexture", &HandleGage::GetHandleTexture, &HandleGage::SetHandleTexture)
		.property("HandleSize", &HandleGage::GetHandleSize, &HandleGage::SetHandleSize)
		.property("HandleOffset", &HandleGage::GetHandleOffset, &HandleGage::SetHandleOffset)
		.property("HandlePivot", &HandleGage::GetHandlePivot, &HandleGage::SetHandlePivot)
		.property("HandleClampPadding", &HandleGage::GetHandleClampPadding, &HandleGage::SetHandleClampPadding)
		.property("HideHandleWhenZero", &HandleGage::GetHideHandleWhenZero, &HandleGage::SetHideHandleWhenZero)
		.property("Value", &HandleGage::GetValue, &HandleGage::SetValue)
		.property("FillDirection", &HandleGage::GetFillDirection, &HandleGage::SetFillDirection)
		.property("OnValueChanged", &HandleGage::GetOnValueChanged, &HandleGage::SetOnValueChanged);

	registration::class_<ObjPtr<HandleGage>>("ObjPtr<HandleGage>")
		.constructor<>([]() { return Object::NewObject<HandleGage>(); })
		.method("Inject", &ObjPtr<HandleGage>::Inject);
}

void MMMEngine::HandleGage::SetBackgroundTexture(const ResPtr<Texture2D>& tex)
{
	m_backgroundTexture = tex;
}

void MMMEngine::HandleGage::SetFillTexture(const ResPtr<Texture2D>& tex)
{
	m_fillTexture = tex;
}

void MMMEngine::HandleGage::SetHandleTexture(const ResPtr<Texture2D>& tex)
{
	m_handleTexture = tex;
}

void MMMEngine::HandleGage::SetNativeSize()
{
	if (m_backgroundTexture)
	{
		ApplyNativeSizeFromTexture(m_backgroundTexture);
		return;
	}
	if (m_fillTexture)
	{
		ApplyNativeSizeFromTexture(m_fillTexture);
		return;
	}
	if (m_handleTexture)
	{
		ApplyNativeSizeFromTexture(m_handleTexture);
	}
}

void MMMEngine::HandleGage::SetValue(float v)
{
	m_value = (v < 0.0f) ? 0.0f : (v > 1.0f) ? 1.0f : v;
}

static DirectX::SimpleMath::Vector2 ComputeHandleSize(const MMMEngine::HandleGage& gage,
	const DirectX::SimpleMath::Vector4& rectCanvas)
{
	using namespace DirectX::SimpleMath;
	using MMMEngine::GageFillDirection;

	const bool horizontal =
		(gage.GetFillDirection() == GageFillDirection::LeftToRight) ||
		(gage.GetFillDirection() == GageFillDirection::RightToLeft);

	Vector2 handleSize = gage.GetHandleSize();
	const float thickness = horizontal ? rectCanvas.w : rectCanvas.z;
	if (handleSize.x <= 0.0f)
		handleSize.x = thickness;
	if (handleSize.y <= 0.0f)
		handleSize.y = thickness;
	return handleSize;
}

static DirectX::SimpleMath::Vector4 ComputeHandleRectCanvas(const MMMEngine::HandleGage& gage,
	const DirectX::SimpleMath::Vector4& rectCanvas,
	float value)
{
	using namespace DirectX::SimpleMath;
	using MMMEngine::GageFillDirection;

	float v = (value < 0.0f) ? 0.0f : (value > 1.0f) ? 1.0f : value;
	const bool horizontal =
		(gage.GetFillDirection() == GageFillDirection::LeftToRight) ||
		(gage.GetFillDirection() == GageFillDirection::RightToLeft);

	const Vector2 handleSize = ComputeHandleSize(gage, rectCanvas);
	const Vector2 pivot = gage.GetHandlePivot();
	const Vector2 offset = gage.GetHandleOffset();
	const Vector2 clampPad = gage.GetHandleClampPadding();

	Vector4 handleRect = {};
	handleRect.z = handleSize.x;
	handleRect.w = handleSize.y;

	if (horizontal)
	{
		const float minPivotX = rectCanvas.x + clampPad.x;
		float maxPivotX = rectCanvas.x + rectCanvas.z - clampPad.y;
		if (maxPivotX < minPivotX)
			maxPivotX = minPivotX;

		float t = v;
		if (gage.GetFillDirection() == GageFillDirection::RightToLeft)
			t = 1.0f - v;

		const float pivotX = minPivotX + (maxPivotX - minPivotX) * t;
		handleRect.x = pivotX - handleSize.x * pivot.x + offset.x;

		const float centerY = rectCanvas.y + rectCanvas.w * 0.5f;
		handleRect.y = centerY - handleSize.y * pivot.y + offset.y;
	}
	else
	{
		const float minPivotY = rectCanvas.y + clampPad.x;
		float maxPivotY = rectCanvas.y + rectCanvas.w - clampPad.y;
		if (maxPivotY < minPivotY)
			maxPivotY = minPivotY;

		float t = v;
		if (gage.GetFillDirection() == GageFillDirection::BottomToTop)
			t = 1.0f - v;

		const float pivotY = minPivotY + (maxPivotY - minPivotY) * t;
		handleRect.y = pivotY - handleSize.y * pivot.y + offset.y;

		const float centerX = rectCanvas.x + rectCanvas.z * 0.5f;
		handleRect.x = centerX - handleSize.x * pivot.x + offset.x;
	}
	return handleRect;
}

void MMMEngine::HandleGage::RenderUI(RenderManager& renderer)
{
	if (!GetCanvas().IsValid())
		return;

	auto rectTransform = GetRectTransform();
	if (!rectTransform.IsValid())
		return;

	auto canvasSize = GetCanvas()->GetCanvasSize();
	auto rectCanvas = rectTransform->GetRectInCanvas(canvasSize);

	auto scale = GetCanvas()->GetScaleToScene();
	Vector4 rect = rectCanvas;
	rect.x *= scale.x;
	rect.y *= scale.y;
	rect.z *= scale.x;
	rect.w *= scale.y;
	const auto offset = GetCanvas()->GetSceneOffset();
	rect.x += offset.x;
	rect.y += offset.y;

	const auto pivot = rectTransform->GetPivot();
	const DirectX::SimpleMath::Vector2 pivotScene = {
		rect.x + rect.z * pivot.x,
		rect.y + rect.w * pivot.y
	};
	using namespace DirectX::SimpleMath;
	const auto worldMat = rectTransform->GetWorldMatrix();
	Vector2 rightDir = { worldMat._11, worldMat._12 };
	Vector2 upDir = { worldMat._21, worldMat._22 };
	const float rightLen = std::sqrt(rightDir.x * rightDir.x + rightDir.y * rightDir.y);
	const float upLen = std::sqrt(upDir.x * upDir.x + upDir.y * upDir.y);
	if (rightLen > 1e-6f) rightDir /= rightLen; else rightDir = { 1.0f, 0.0f };
	if (upLen > 1e-6f) upDir /= upLen; else upDir = { 0.0f, 1.0f };
	auto makePivotN = [](const DirectX::SimpleMath::Vector4& r,
		const DirectX::SimpleMath::Vector2& pivotScenePos)
	{
		return DirectX::SimpleMath::Vector2{
			r.z != 0.0f ? (pivotScenePos.x - r.x) / r.z : 0.0f,
			r.w != 0.0f ? (pivotScenePos.y - r.y) / r.w : 0.0f
		};
	};
	const auto pivotN = makePivotN(rect, pivotScene);

	auto applyOffsetScaleAroundPivot = [&](const Vector4& baseRect,
		const Vector2& pivot01,
		const Vector2& offsetScene,
		const Vector2& scaleXY)
	{
		Vector4 r = baseRect;
		// 피벗 기준으로 스케일 적용
		const float pivotX = r.x + r.z * pivot01.x;
		const float pivotY = r.y + r.w * pivot01.y;
		r.z *= scaleXY.x;
		r.w *= scaleXY.y;
		r.x = pivotX - r.z * pivot01.x;
		r.y = pivotY - r.w * pivot01.y;
		// 그 후에 오프셋 추가
		r.x += offsetScene.x;
		r.y += offsetScene.y;
		return r;
	};

	// 배경
	Vector4 bgRect = applyOffsetScaleAroundPivot(rect, pivot, m_backgroundOffset, m_backgroundScale);
	const auto bgPivotN = makePivotN(bgRect, pivotScene);
	renderer.DrawUIElement(bgRect, { 0.0f, 0.0f, 1.0f, 1.0f }, GetColor(),
		m_backgroundTexture ? m_backgroundTexture : m_fillTexture,
		bgPivotN, rightDir, upDir);

	float v = (m_value < 0.0f) ? 0.0f : (m_value > 1.0f) ? 1.0f : m_value;
	if (v > 0.0f && m_fillTexture)
	{
		Vector4 fillBaseRect = rect;
		fillBaseRect.x += m_fillOffset.x;
		fillBaseRect.y += m_fillOffset.y;
		fillBaseRect.z *= m_fillScale.x;
		fillBaseRect.w *= m_fillScale.y;
		Vector4 fillRect = fillBaseRect;
		Vector4 fillUV = { 0.0f, 0.0f, 1.0f, 1.0f };

		switch (m_fillDirection)
		{
		case GageFillDirection::LeftToRight:
			fillRect.z = fillBaseRect.z * v;
			fillUV.z = v;
			break;
		case GageFillDirection::RightToLeft:
			fillRect.x = fillBaseRect.x + fillBaseRect.z * (1.0f - v);
			fillRect.z = fillBaseRect.z * v;
			fillUV.x = 1.0f - v;
			fillUV.z = 1.0f;
			break;
		case GageFillDirection::BottomToTop:
			fillRect.y = fillBaseRect.y + fillBaseRect.w * (1.0f - v);
			fillRect.w = fillBaseRect.w * v;
			fillUV.y = 1.0f - v;
			fillUV.w = 1.0f;
			break;
		case GageFillDirection::TopToBottom:
			fillRect.w = fillBaseRect.w * v;
			fillUV.w = v;
			break;
		}

		const auto fillPivotN = makePivotN(fillRect, pivotScene);
		renderer.DrawUIElement(fillRect, fillUV, GetColor(), m_fillTexture, fillPivotN, rightDir, upDir);
	}

	if (!m_handleTexture)
		return;
	if (m_hideHandleWhenZero && v <= 0.0f)
		return;

	Vector4 handleRectCanvas = ComputeHandleRectCanvas(*this, rectCanvas, v);
	Vector4 handleRect = handleRectCanvas;
	handleRect.x *= scale.x;
	handleRect.y *= scale.y;
	handleRect.z *= scale.x;
	handleRect.w *= scale.y;
	handleRect.x += offset.x;
	handleRect.y += offset.y;

	const auto handlePivotN = makePivotN(handleRect, pivotScene);
	renderer.DrawUIElement(handleRect, { 0.0f, 0.0f, 1.0f, 1.0f }, GetColor(), m_handleTexture,
		handlePivotN, rightDir, upDir);
}

static bool PointInRect(float px, float py, float rx, float ry, float rw, float rh)
{
	return px >= rx && px <= rx + rw && py >= ry && py <= ry + rh;
}

static bool TryGetPointerLocal(
	const MMMEngine::ObjPtr<MMMEngine::RectTransform>& rectTr,
	const DirectX::SimpleMath::Vector2& canvasSize,
	const DirectX::SimpleMath::Vector2& pointerInCanvas,
	DirectX::SimpleMath::Vector2& outLocal01)
{
	using namespace DirectX::SimpleMath;
	auto rect = rectTr->GetRectInCanvas(canvasSize);
	const auto pivot = rectTr->GetPivot();
	const Vector2 pivotPos = { rect.x + rect.z * pivot.x, rect.y + rect.w * pivot.y };

	const auto worldMat = rectTr->GetWorldMatrix();
	Vector2 rightDir = { worldMat._11, worldMat._12 };
	Vector2 upDir = { worldMat._21, worldMat._22 };
	const float rightLen = std::sqrt(rightDir.x * rightDir.x + rightDir.y * rightDir.y);
	const float upLen = std::sqrt(upDir.x * upDir.x + upDir.y * upDir.y);
	if (rightLen > 1e-6f) rightDir /= rightLen; else rightDir = { 1.0f, 0.0f };
	if (upLen > 1e-6f) upDir /= upLen; else upDir = { 0.0f, 1.0f };

	const float det = rightDir.x * upDir.y - rightDir.y * upDir.x;
	if (std::abs(det) < 1e-6f)
		return false;

	const Vector2 d = pointerInCanvas - pivotPos;
	const float invDet = 1.0f / det;
	const float localX = (d.x * upDir.y - d.y * upDir.x) * invDet;
	const float localY = (-d.x * rightDir.y + d.y * rightDir.x) * invDet;

	outLocal01.x = rect.z > 1e-6f ? (localX / rect.z + pivot.x) : pivot.x;
	outLocal01.y = rect.w > 1e-6f ? (localY / rect.w + pivot.y) : pivot.y;
	return true;
}

void MMMEngine::HandleGage::UpdatePointer(const DirectX::SimpleMath::Vector2& canvasSize,
	const DirectX::SimpleMath::Vector2& pointerInCanvas,
	bool isMouseDown)
{
	auto rectTr = GetRectTransform();
	if (!rectTr.IsValid())
		return;

	auto rect = rectTr->GetRectInCanvas(canvasSize);
	DirectX::SimpleMath::Vector2 local01;
	bool hasLocal = TryGetPointerLocal(rectTr, canvasSize, pointerInCanvas, local01);
	const float localX = local01.x * rect.z;
	const float localY = local01.y * rect.w;
	bool inside = false;
	if (hasLocal)
		inside = PointInRect(localX, localY, 0.0f, 0.0f, rect.z, rect.w);
	else
		inside = PointInRect(pointerInCanvas.x, pointerInCanvas.y, rect.x, rect.y, rect.z, rect.w);
	if (!inside && m_handleTexture && !(m_hideHandleWhenZero && GetValue() <= 0.0f))
	{
		const DirectX::SimpleMath::Vector4 rectLocal = { 0.0f, 0.0f, rect.z, rect.w };
		auto handleRect = ComputeHandleRectCanvas(*this, rectLocal, GetValue());
		if (hasLocal)
		{
			inside = PointInRect(localX, localY,
				handleRect.x, handleRect.y, handleRect.z, handleRect.w);
		}
		else
		{
			// Fallback to axis-aligned canvas space
			auto handleRectCanvas = ComputeHandleRectCanvas(*this, rect, GetValue());
			inside = PointInRect(pointerInCanvas.x, pointerInCanvas.y,
				handleRectCanvas.x, handleRectCanvas.y, handleRectCanvas.z, handleRectCanvas.w);
		}
	}

	if (isMouseDown && inside)
		m_isDragging = true;
	if (!isMouseDown)
		m_isDragging = false;

	if (!m_isDragging)
		return;

	float newValue = GetValue();
	const bool horizontal =
		(GetFillDirection() == GageFillDirection::LeftToRight) ||
		(GetFillDirection() == GageFillDirection::RightToLeft);

	if (m_handleTexture)
	{
		const auto handleSize = ComputeHandleSize(*this, rect);
		const auto pivot = GetHandlePivot();
		const auto offset = GetHandleOffset();
		const auto clampPad = GetHandleClampPadding();
		const float ptrX = hasLocal ? localX : pointerInCanvas.x;
		const float ptrY = hasLocal ? localY : pointerInCanvas.y;
		const DirectX::SimpleMath::Vector4 rectLocal = hasLocal
			? DirectX::SimpleMath::Vector4(0.0f, 0.0f, rect.z, rect.w)
			: rect;

		if (horizontal)
		{
			float minPivotX = rectLocal.x + clampPad.x;
			float maxPivotX = rectLocal.x + rectLocal.z - clampPad.y;
			if (maxPivotX < minPivotX)
				maxPivotX = minPivotX;

			const float denom = maxPivotX - minPivotX;
			if (denom > 0.0f)
			{
				const float pivotX = ptrX - offset.x;
				newValue = (pivotX - minPivotX) / denom;
			}
			else
			{
				newValue = 0.0f;
			}
		}
		else
		{
			float minPivotY = rectLocal.y + clampPad.x;
			float maxPivotY = rectLocal.y + rectLocal.w - clampPad.y;
			if (maxPivotY < minPivotY)
				maxPivotY = minPivotY;

			const float denom = maxPivotY - minPivotY;
			if (denom > 0.0f)
			{
				const float pivotY = ptrY - offset.y;
				newValue = (pivotY - minPivotY) / denom;
			}
			else
			{
				newValue = 0.0f;
			}
		}
	}
	else
	{
		switch (GetFillDirection())
		{
		case GageFillDirection::LeftToRight:
		case GageFillDirection::RightToLeft:
			if (rect.z > 0.0f)
				newValue = hasLocal ? (localX / rect.z) : (pointerInCanvas.x - rect.x) / rect.z;
			break;
		case GageFillDirection::BottomToTop:
		case GageFillDirection::TopToBottom:
			if (rect.w > 0.0f)
				newValue = hasLocal ? (localY / rect.w) : (pointerInCanvas.y - rect.y) / rect.w;
			break;
		}
	}

	if (newValue < 0.0f) newValue = 0.0f;
	if (newValue > 1.0f) newValue = 1.0f;

	if (GetFillDirection() == GageFillDirection::RightToLeft || GetFillDirection() == GageFillDirection::BottomToTop)
		newValue = 1.0f - newValue;

	if (newValue != GetValue())
	{
		SetValue(newValue);
		m_onValueChanged.Invoke(newValue);
	}
}
