#include "Button.h"
#include "RectTransform.h"
#include "Canvas.h"
#include "Texture2D.h"
#include "rttr/registration"

RTTR_REGISTRATION
{
	using namespace rttr;
	using namespace MMMEngine;

	registration::class_<Button>("Button")
		(rttr::metadata("wrapper_type_name", "ObjPtr<Button>"))
		.property("Color", &Button::GetColor, &Button::SetColor)
		.property("Texture", &Button::GetTexture, &Button::SetTexture)
		.property("OnHoverEnter", &Button::GetOnHoverEnter, &Button::SetOnHoverEnter)
		.property("OnHoverStay", &Button::GetOnHoverStay, &Button::SetOnHoverStay)
		.property("OnHoverExit", &Button::GetOnHoverExit, &Button::SetOnHoverExit)
		.property("OnClick", &Button::GetOnClick, &Button::SetOnClick)
		.property("OnRelease", &Button::GetOnRelease, &Button::SetOnRelease);

	registration::class_<ObjPtr<Button>>("ObjPtr<Button>")
		.constructor<>([]() { return Object::NewObject<Button>(); })
		.method("Inject", &ObjPtr<Button>::Inject);
}

void MMMEngine::Button::SetTexture(const ResPtr<Texture2D>& texture)
{
	Graphic::SetTexture(texture);
}

void MMMEngine::Button::SetNativeSize()
{
	ApplyNativeSizeFromTexture(GetTexture());
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

void MMMEngine::Button::UpdatePointer(const DirectX::SimpleMath::Vector2& canvasSize,
	const DirectX::SimpleMath::Vector2& pointerInCanvas,
	bool isMouseDown)
{
	auto rectTr = GetRectTransform();
	if (!rectTr.IsValid())
		return;

	auto rect = rectTr->GetRectInCanvas(canvasSize);
	bool inside = false;
	DirectX::SimpleMath::Vector2 local01;
	if (TryGetPointerLocal(rectTr, canvasSize, pointerInCanvas, local01))
	{
		inside = local01.x >= 0.0f && local01.x <= 1.0f
			&& local01.y >= 0.0f && local01.y <= 1.0f;
	}
	else
	{
		inside = PointInRect(pointerInCanvas.x, pointerInCanvas.y, rect.x, rect.y, rect.z, rect.w);
	}

	if (inside)
	{
		if (!m_isHovered)
		{
			m_isHovered = true;
			m_onHoverEnter.Invoke();
		}
		else
			m_onHoverStay.Invoke();

		if (isMouseDown)
		{
			if (!m_isPressed)
			{
				m_isPressed = true;
				m_onClick.Invoke();
			}
		}
		else
		{
			if (m_isPressed)
			{
				m_isPressed = false;
				m_onRelease.Invoke();
			}
		}
	}
	else
	{
		if (m_isHovered)
		{
			m_isHovered = false;
			m_onHoverExit.Invoke();
		}
		if (m_isPressed)
			m_isPressed = false;
	}
}
