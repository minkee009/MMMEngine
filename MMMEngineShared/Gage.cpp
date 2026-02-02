#include "Gage.h"
#include "Canvas.h"
#include "RectTransform.h"
#include "RenderManager.h"
#include "Texture2D.h"
#include "rttr/registration"

RTTR_REGISTRATION
{
	using namespace rttr;
	using namespace MMMEngine;

	registration::enumeration<GageFillDirection>("GageFillDirection")
		(
			value("LeftToRight", GageFillDirection::LeftToRight),
			value("RightToLeft", GageFillDirection::RightToLeft),
			value("BottomToTop", GageFillDirection::BottomToTop),
			value("TopToBottom", GageFillDirection::TopToBottom)
		);

	registration::class_<Gage>("Gage")
		(rttr::metadata("wrapper_type_name", "ObjPtr<Gage>"))
		.property("Color", &Gage::GetColor, &Gage::SetColor)
		.property("BackgroundTexture", &Gage::GetBackgroundTexture, &Gage::SetBackgroundTexture)
		.property("FillTexture", &Gage::GetFillTexture, &Gage::SetFillTexture)
		.property("Value", &Gage::GetValue, &Gage::SetValue)
		.property("FillDirection", &Gage::GetFillDirection, &Gage::SetFillDirection);

	registration::class_<ObjPtr<Gage>>("ObjPtr<Gage>")
		.constructor<>([]() { return Object::NewObject<Gage>(); })
		.method("Inject", &ObjPtr<Gage>::Inject);
}

void MMMEngine::Gage::SetValue(float v)
{
	m_value = (v < 0.0f) ? 0.0f : (v > 1.0f) ? 1.0f : v;
}

void MMMEngine::Gage::RenderUI(RenderManager& renderer)
{
	if (!GetCanvas().IsValid())
		return;

	auto rectTransform = GetRectTransform();
	if (!rectTransform.IsValid())
		return;

	auto canvasSize = GetCanvas()->GetCanvasSize();
	auto rect = rectTransform->GetRectInCanvas(canvasSize);

	auto scale = GetCanvas()->GetScaleToScene();
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
	const auto worldMat = rectTransform->GetWorldMatrix();
	DirectX::SimpleMath::Vector2 rightDir = { worldMat._11, worldMat._12 };
	DirectX::SimpleMath::Vector2 upDir = { worldMat._21, worldMat._22 };
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

	// 배경
	renderer.DrawUIElement(rect, { 0.0f, 0.0f, 1.0f, 1.0f }, GetColor(),
		m_backgroundTexture ? m_backgroundTexture : m_fillTexture,
		pivotN, rightDir, upDir);

	float v = (m_value < 0.0f) ? 0.0f : (m_value > 1.0f) ? 1.0f : m_value;
	if (v <= 0.0f || !m_fillTexture)
		return;

	using namespace DirectX::SimpleMath;
	Vector4 fillRect = rect;
	Vector4 fillUV = { 0.0f, 0.0f, 1.0f, 1.0f };

	switch (m_fillDirection)
	{
	case GageFillDirection::LeftToRight:
		fillRect.z = rect.z * v;
		fillUV.z = v;
		break;
	case GageFillDirection::RightToLeft:
		fillRect.x = rect.x + rect.z * (1.0f - v);
		fillRect.z = rect.z * v;
		fillUV.x = 1.0f - v;
		fillUV.z = 1.0f;
		break;
	case GageFillDirection::BottomToTop:
		fillRect.y = rect.y + rect.w * (1.0f - v);
		fillRect.w = rect.w * v;
		fillUV.y = 1.0f - v;
		fillUV.w = 1.0f;
		break;
	case GageFillDirection::TopToBottom:
		fillRect.w = rect.w * v;
		fillUV.w = v;
		break;
	}

	const auto fillPivotN = makePivotN(fillRect, pivotScene);
	renderer.DrawUIElement(fillRect, fillUV, GetColor(), m_fillTexture, fillPivotN, rightDir, upDir);
}
