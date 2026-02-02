#pragma once

#include "Export.h"
#include "Graphic.h"
#include "ResourceManager.h"
#include "SimpleMath.h"
#include <string>

namespace MMMEngine
{
	class Font;
	class RenderManager;

	/// 텍스트 정렬
	enum class TextAlignment
	{
		Left,
		Center,
		Right,
	};

	/// 줄바꿈 방식
	enum class TextWrapMode
	{
		NoWrap,      /// 줄바꿈 없음
		WordWrap,    /// 단어 단위 줄바꿈
		CharacterWrap,
	};

	/// 폰트 스타일 (비트플래그 가능)
	enum class FontStyle
	{
		Normal = 0,
		Bold = 1,
		Italic = 2,
		BoldItalic = 3,
	};

	/// 텍스트 UI. wstring 출력, 폰트·정렬·줄바꿈·스타일 설정.
	/// 실제 그리기는 RenderManager::DrawUIText 구현 후 연동.
	class MMMENGINE_API Text : public Graphic
	{
	private:
		RTTR_ENABLE(Graphic)
		RTTR_REGISTRATION_FRIEND

		std::wstring m_text;
		ResPtr<Font> m_font = nullptr;
		TextAlignment m_alignment = TextAlignment::Center;
		TextWrapMode m_wrapMode = TextWrapMode::WordWrap;
		FontStyle m_fontStyle = FontStyle::Normal;

	public:
		Text() = default;
		virtual ~Text() = default;

		const std::wstring& GetText() const { return m_text; }
		void SetText(const std::wstring& text) { m_text = text; }
		std::string GetTextUtf8() const;
		void SetTextUtf8(std::string text);

		const ResPtr<Font>& GetFont() const { return m_font; }
		void SetFont(const ResPtr<Font>& font) { m_font = font; }

		TextAlignment GetAlignment() const { return m_alignment; }
		void SetAlignment(TextAlignment a) { m_alignment = a; }

		TextWrapMode GetWrapMode() const { return m_wrapMode; }
		void SetWrapMode(TextWrapMode m) { m_wrapMode = m; }

		FontStyle GetFontStyle() const { return m_fontStyle; }
		void SetFontStyle(FontStyle s) { m_fontStyle = s; }

		void RenderUI(RenderManager& renderer) override;
	};
}
