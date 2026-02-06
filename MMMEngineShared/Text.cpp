#include "Text.h"
#include "Font.h"
#include "Canvas.h"
#include "RectTransform.h"
#include "RenderManager.h"
#include "StringHelper.h"
#include "rttr/registration"

RTTR_REGISTRATION
{
	using namespace rttr;
	using namespace MMMEngine;

	registration::enumeration<TextAlignment>("TextAlignment")
		(value("Left", TextAlignment::Left),
		 value("Center", TextAlignment::Center),
		 value("Right", TextAlignment::Right));

	registration::enumeration<TextWrapMode>("TextWrapMode")
		(value("NoWrap", TextWrapMode::NoWrap),
		 value("WordWrap", TextWrapMode::WordWrap),
		 value("CharacterWrap", TextWrapMode::CharacterWrap));

	registration::enumeration<FontStyle>("FontStyle")
		(value("Normal", FontStyle::Normal),
		 value("Bold", FontStyle::Bold),
		 value("Italic", FontStyle::Italic),
		 value("BoldItalic", FontStyle::BoldItalic));

	registration::class_<Text>("Text")
		(rttr::metadata("wrapper_type_name", "ObjPtr<Text>"))
		.property("Text", &Text::GetText, &Text::SetText)
		.property("TextUtf8", &Text::GetTextUtf8, &Text::SetTextUtf8)
		.property("Font", &Text::GetFont, &Text::SetFont)
		.property("Color", &Text::GetColor, &Text::SetColor)
		.property("Alignment", &Text::GetAlignment, &Text::SetAlignment)
		.property("WrapMode", &Text::GetWrapMode, &Text::SetWrapMode)
		.property("FontStyle", &Text::GetFontStyle, &Text::SetFontStyle);

	registration::class_<ObjPtr<Text>>("ObjPtr<Text>")
		.constructor<>([]() { return Object::NewObject<Text>(); })
		.method("Inject", &ObjPtr<Text>::Inject);
}

void MMMEngine::Text::RenderUI(RenderManager& renderer)
{
	if (m_text.empty() || !m_font)
		return;

	auto canvas = GetCanvas();
	if (!canvas.IsValid())
		return;

	auto rectTransform = GetRectTransform();
	if (!rectTransform.IsValid())
		return;

	auto canvasSize = canvas->GetCanvasSize();
	auto rect = rectTransform->GetRectInCanvas(canvasSize);

	// 캔버스 좌표를 씬 픽셀로 변환 (ConstantPixelSize=1:1, ScaleWithScreenSize=ref기준 스케일)
	auto scale = canvas->GetScaleToScene();
	rect.x *= scale.x;
	rect.y *= scale.y;
	rect.z *= scale.x;
	rect.w *= scale.y;
	const auto offset = canvas->GetSceneOffset();
	rect.x += offset.x;
	rect.y += offset.y;

	const auto pivot = rectTransform->GetPivot();
	const DirectX::SimpleMath::Vector2 pivotScene = {
		rect.x + rect.z * pivot.x,
		rect.y + rect.w * pivot.y
	};
	const auto rotEuler = rectTransform->GetWorldEulerRotation();
	const float rotationRad = DirectX::XMConvertToRadians(rotEuler.z);

	renderer.DrawUIText(rect, m_text, m_font, GetColor(), m_alignment, rotationRad, pivotScene);
}

std::string MMMEngine::Text::GetTextUtf8() const
{
	return Utility::StringHelper::WStringToString(m_text);
}

void MMMEngine::Text::SetTextUtf8(std::string text)
{
	m_text = Utility::StringHelper::StringToWString(text);
}
