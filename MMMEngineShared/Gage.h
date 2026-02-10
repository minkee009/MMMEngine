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

		// 배경 / 게이지 별 오프셋 & 스케일
		const DirectX::SimpleMath::Vector2& GetBackgroundOffset() const { return m_backgroundOffset; }
		void SetBackgroundOffset(const DirectX::SimpleMath::Vector2& v) { m_backgroundOffset = v; }

		const DirectX::SimpleMath::Vector2& GetBackgroundScale() const { return m_backgroundScale; }
		void SetBackgroundScale(const DirectX::SimpleMath::Vector2& v) { m_backgroundScale = v; }

		const DirectX::SimpleMath::Vector2& GetFillOffset() const { return m_fillOffset; }
		void SetFillOffset(const DirectX::SimpleMath::Vector2& v) { m_fillOffset = v; }

		const DirectX::SimpleMath::Vector2& GetFillScale() const { return m_fillScale; }
		void SetFillScale(const DirectX::SimpleMath::Vector2& v) { m_fillScale = v; }

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

	private:
		DirectX::SimpleMath::Vector2 m_backgroundOffset = { 0.0f, 0.0f };
		DirectX::SimpleMath::Vector2 m_backgroundScale = { 1.0f, 1.0f };
		DirectX::SimpleMath::Vector2 m_fillOffset = { 0.0f, 0.0f };
		DirectX::SimpleMath::Vector2 m_fillScale = { 1.0f, 1.0f };
	};
}
