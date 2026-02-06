#include "RectTransform.h"
#include "Canvas.h"
#include "rttr/registration"
#include "rttr/detail/policies/ctor_policies.h"
#include <cmath>

using namespace DirectX::SimpleMath;

namespace
{
	inline float Clamp01(float v)
	{
		if (v < 0.0f) return 0.0f;
		if (v > 1.0f) return 1.0f;
		return v;
	}

	inline void ComputeBasis2D(const DirectX::SimpleMath::Matrix& worldMat,
		DirectX::SimpleMath::Vector2& rightDir,
		DirectX::SimpleMath::Vector2& upDir)
	{
		using namespace DirectX::SimpleMath;
		rightDir = { worldMat._11, worldMat._12 };
		upDir = { worldMat._21, worldMat._22 };
		const float rightLen = std::sqrt(rightDir.x * rightDir.x + rightDir.y * rightDir.y);
		const float upLen = std::sqrt(upDir.x * upDir.x + upDir.y * upDir.y);
		if (rightLen > 1e-6f) rightDir /= rightLen; else rightDir = { 1.0f, 0.0f };
		if (upLen > 1e-6f) upDir /= upLen; else upDir = { 0.0f, 1.0f };
	}
}

RTTR_REGISTRATION
{
	using namespace rttr;
	using namespace MMMEngine;

	registration::class_<RectTransform>("RectTransform")
		(rttr::metadata("wrapper_type_name", "ObjPtr<RectTransform>"))
		(rttr::metadata("INSPECTOR", "DONT_ADD_COMP"))
		.property("AnchoredPosition", &RectTransform::GetAnchoredPosition, &RectTransform::SetAnchoredPosition)
		.property("AnchorMin", &RectTransform::GetAnchorMin, &RectTransform::SetAnchorMin)
		.property("AnchorMax", &RectTransform::GetAnchorMax, &RectTransform::SetAnchorMax)
		.property("Pivot", &RectTransform::GetPivot, &RectTransform::SetPivot)
		.property("SizeDelta", &RectTransform::GetSizeDelta, &RectTransform::SetSizeDelta)
			(rttr::metadata("INSPECTOR", "HIDDEN"))
		.property("Width", &RectTransform::GetWidth, &RectTransform::SetWidth)
		.property("Height", &RectTransform::GetHeight, &RectTransform::SetHeight);

	registration::class_<ObjPtr<RectTransform>>("ObjPtr<RectTransform>")
		.constructor<>([]() {
			return Object::NewObject<RectTransform>();
		})
		.method("Inject", &ObjPtr<RectTransform>::Inject);
}

MMMEngine::RectTransform::RectTransform()
	: Transform()
{
}

Vector2 MMMEngine::RectTransform::GetAnchoredPosition() const
{
	const auto pos = GetLocalPosition();
	return { pos.x, pos.y };
}

void MMMEngine::RectTransform::SetAnchoredPosition(Vector2 pos)
{
	const auto current = GetLocalPosition();
	SetLocalPosition({ pos.x, pos.y, current.z });
}

void MMMEngine::RectTransform::SetAnchorMin(const Vector2& v)
{
	m_anchorMin = { Clamp01(v.x), Clamp01(v.y) };
}

void MMMEngine::RectTransform::SetAnchorMax(const Vector2& v)
{
	m_anchorMax = { Clamp01(v.x), Clamp01(v.y) };
}

void MMMEngine::RectTransform::SetPivot(const Vector2& v)
{
	m_pivot = v;
}

void MMMEngine::RectTransform::SetSizeDelta(const Vector2& v)
{
	m_sizeDelta = v;
}

void MMMEngine::RectTransform::SetWidth(float w)
{
	m_sizeDelta.x = w;
}

void MMMEngine::RectTransform::SetHeight(float h)
{
	m_sizeDelta.y = h;
}

Vector4 MMMEngine::RectTransform::GetRectInCanvas(const Vector2& canvasSize) const
{
	if (auto go = GetGameObject(); go.IsValid())
	{
		if (auto canvas = go->GetComponent<Canvas>(); canvas.IsValid())
		{
			return Vector4(0.0f, 0.0f, canvasSize.x, canvasSize.y);
		}
	}

	Vector2 parentSize = canvasSize;
	Vector2 parentOrigin = Vector2::Zero;
	Vector2 parentRight = Vector2::UnitX;
	Vector2 parentUp = Vector2::UnitY;

	if (auto parent = GetParent())
	{
		if (auto parentRect = parent.Cast<RectTransform>())
		{
			Vector4 rect = parentRect->GetRectInCanvas(canvasSize);
			parentSize = { rect.z, rect.w };

			const auto pivot = parentRect->GetPivot();
			const Vector2 parentPivotPos = {
				rect.x + rect.z * pivot.x,
				rect.y + rect.w * pivot.y
			};

			ComputeBasis2D(parentRect->GetWorldMatrix(), parentRight, parentUp);

			parentOrigin = parentPivotPos
				+ parentRight * (-parentSize.x * pivot.x)
				+ parentUp * (-parentSize.y * pivot.y);
		}
	}

	const Vector2 anchorMinLocal = { parentSize.x * m_anchorMin.x,
		parentSize.y * m_anchorMin.y };
	const Vector2 anchorMaxLocal = { parentSize.x * m_anchorMax.x,
		parentSize.y * m_anchorMax.y };

	const Vector2 anchorCenterLocal = (anchorMinLocal + anchorMaxLocal) * 0.5f;
	const Vector2 anchorCenterWorld = parentOrigin
		+ parentRight * anchorCenterLocal.x
		+ parentUp * anchorCenterLocal.y;

	Vector2 size = (anchorMaxLocal - anchorMinLocal) + m_sizeDelta;

	const auto scale = GetWorldScale();
	size.x *= scale.x;
	size.y *= scale.y;

	const Vector2 anchoredPos = GetAnchoredPosition();
	Vector2 pivotPos = anchorCenterWorld
		+ parentRight * anchoredPos.x
		+ parentUp * anchoredPos.y;
	Vector2 rectMin = pivotPos - Vector2(size.x * m_pivot.x, size.y * m_pivot.y);

	return Vector4(rectMin.x, rectMin.y, size.x, size.y);
}

Vector2 MMMEngine::RectTransform::GetPivotPositionInCanvas(const Vector2& canvasSize) const
{
	const auto rect = GetRectInCanvas(canvasSize);
	return {
		rect.x + rect.z * m_pivot.x,
		rect.y + rect.w * m_pivot.y
	};
}

Vector2 MMMEngine::RectTransform::GetAnchoredPositionInCanvas(const Vector2& canvasSize) const
{
	return GetPivotPositionInCanvas(canvasSize);
}

void MMMEngine::RectTransform::GetAnchorData(const Vector2& canvasSize,
	Vector2& anchorCenter,
	Vector2& anchorSpan) const
{
	Vector2 parentSize = canvasSize;
	Vector2 parentOrigin = Vector2::Zero;
	Vector2 parentRight = Vector2::UnitX;
	Vector2 parentUp = Vector2::UnitY;

	if (auto parent = GetParent())
	{
		if (auto parentRect = parent.Cast<RectTransform>())
		{
			Vector4 rect = parentRect->GetRectInCanvas(canvasSize);
			parentSize = { rect.z, rect.w };

			const auto pivot = parentRect->GetPivot();
			const Vector2 parentPivotPos = {
				rect.x + rect.z * pivot.x,
				rect.y + rect.w * pivot.y
			};
			ComputeBasis2D(parentRect->GetWorldMatrix(), parentRight, parentUp);

			parentOrigin = parentPivotPos
				+ parentRight * (-parentSize.x * pivot.x)
				+ parentUp * (-parentSize.y * pivot.y);
		}
	}

	const Vector2 anchorMin = GetAnchorMin();
	const Vector2 anchorMax = GetAnchorMax();
	const Vector2 anchorMinLocal = {
		parentSize.x * anchorMin.x,
		parentSize.y * anchorMin.y
	};
	const Vector2 anchorMaxLocal = {
		parentSize.x * anchorMax.x,
		parentSize.y * anchorMax.y
	};

	const Vector2 anchorCenterLocal = (anchorMinLocal + anchorMaxLocal) * 0.5f;
	anchorCenter = parentOrigin
		+ parentRight * anchorCenterLocal.x
		+ parentUp * anchorCenterLocal.y;
	anchorSpan = { anchorMaxLocal.x - anchorMinLocal.x, anchorMaxLocal.y - anchorMinLocal.y };
}

Vector4 MMMEngine::RectTransform::GetAnchorRectInCanvas(const Vector2& canvasSize) const
{
	Vector2 anchorCenter;
	Vector2 anchorSpan;
	GetAnchorData(canvasSize, anchorCenter, anchorSpan);

	const Vector2 rectMin = {
		anchorCenter.x - anchorSpan.x * 0.5f,
		anchorCenter.y - anchorSpan.y * 0.5f
	};
	return Vector4(rectMin.x, rectMin.y, anchorSpan.x, anchorSpan.y);
}
