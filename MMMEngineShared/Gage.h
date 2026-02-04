#pragma once

#include "Export.h"
#include "Graphic.h"
#include "ResourceManager.h"
#include "SimpleMath.h"

namespace MMMEngine
{
	class Texture2D;

	enum class GageFillDirection
	{
		LeftToRight,
		RightToLeft,
		BottomToTop,
		TopToBottom,
	};

	/// 게이지 바 UI. Graphic 상속. Canvas는 Graphic으로만 등록. 배경/채움 텍스처 + value 0~1.
	class MMMENGINE_API Gage : public Graphic
	{
	private:
		RTTR_ENABLE(Graphic)
		RTTR_REGISTRATION_FRIEND

		ResPtr<Texture2D> m_backgroundTexture = nullptr;
		ResPtr<Texture2D> m_fillTexture = nullptr;
		float m_value = 1.0f;
		GageFillDirection m_fillDirection = GageFillDirection::LeftToRight;

	public:
		Gage() = default;
		virtual ~Gage() = default;

		const ResPtr<Texture2D>& GetBackgroundTexture() const { return m_backgroundTexture; }
		void SetBackgroundTexture(const ResPtr<Texture2D>& tex);

		const ResPtr<Texture2D>& GetFillTexture() const { return m_fillTexture; }
		void SetFillTexture(const ResPtr<Texture2D>& tex);

		void SetNativeSize();

		float GetValue() const { return m_value; }
		void SetValue(float v);

		GageFillDirection GetFillDirection() const { return m_fillDirection; }
		void SetFillDirection(GageFillDirection dir) { m_fillDirection = dir; }

		void RenderUI(RenderManager& renderer) override;
	};
}
