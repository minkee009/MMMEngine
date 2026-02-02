#include "RectTransform.h"
#include "Canvas.h"
#include "rttr/registration"
#include "rttr/detail/policies/ctor_policies.h"

using namespace DirectX::SimpleMath;

namespace
{
	inline float Clamp01(float v)
	{
		if (v < 0.0f) return 0.0f;
		if (v > 1.0f) return 1.0f;
		return v;
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

	Vector2 parentMin = Vector2::Zero;
	Vector2 parentSize = canvasSize;

	if (auto parent = GetParent())
	{
		if (auto parentRect = parent.Cast<RectTransform>())
		{
			Vector4 rect = parentRect->GetRectInCanvas(canvasSize);
			parentMin = { rect.x, rect.y };
			parentSize = { rect.z, rect.w };
		}
	}

	Vector2 anchorMin = { parentMin.x + parentSize.x * m_anchorMin.x,
		parentMin.y + parentSize.y * m_anchorMin.y };
	Vector2 anchorMax = { parentMin.x + parentSize.x * m_anchorMax.x,
		parentMin.y + parentSize.y * m_anchorMax.y };

	Vector2 anchorCenter = (anchorMin + anchorMax) * 0.5f;
	Vector2 size = (anchorMax - anchorMin) + m_sizeDelta;

	const auto scale = GetWorldScale();
	size.x *= scale.x;
	size.y *= scale.y;

	Vector2 pivotPos = anchorCenter + GetAnchoredPosition();
	Vector2 rectMin = pivotPos - Vector2(size.x * m_pivot.x, size.y * m_pivot.y);

	return Vector4(rectMin.x, rectMin.y, size.x, size.y);
}

void MMMEngine::RectTransform::GetAnchorData(const Vector2& canvasSize,
	Vector2& anchorCenter,
	Vector2& anchorSpan) const
{
	Vector2 parentMin = Vector2::Zero;
	Vector2 parentSize = canvasSize;

	if (auto parent = GetParent())
	{
		if (auto parentRect = parent.Cast<RectTransform>())
		{
			Vector4 rect = parentRect->GetRectInCanvas(canvasSize);
			parentMin = { rect.x, rect.y };
			parentSize = { rect.z, rect.w };
		}
	}

	const Vector2 anchorMin = GetAnchorMin();
	const Vector2 anchorMax = GetAnchorMax();
	const Vector2 anchorMinPos = {
		parentMin.x + parentSize.x * anchorMin.x,
		parentMin.y + parentSize.y * anchorMin.y
	};
	const Vector2 anchorMaxPos = {
		parentMin.x + parentSize.x * anchorMax.x,
		parentMin.y + parentSize.y * anchorMax.y
	};

	anchorCenter = (anchorMinPos + anchorMaxPos) * 0.5f;
	anchorSpan = { anchorMaxPos.x - anchorMinPos.x, anchorMaxPos.y - anchorMinPos.y };
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
