#pragma once

#include "Export.h"
#include "Graphic.h"
#include "SimpleMath.h"

namespace MMMEngine
{
	/// 텍스처 하나를 그리는 UI. 알파·베이스컬러 조정 가능.
	class MMMENGINE_API Image : public Graphic
	{
	private:
		RTTR_ENABLE(Graphic)
		RTTR_REGISTRATION_FRIEND

	public:
		Image() = default;
		virtual ~Image() = default;

		/// 베이스 컬러 (RGB). Graphic::GetColor/SetColor와 동일.
		const DirectX::SimpleMath::Color& GetColor() const { return Graphic::GetColor(); }
		void SetColor(const DirectX::SimpleMath::Color& color) { Graphic::SetColor(color); }

		/// 알파값 (0~1). Color.w
		float GetAlpha() const { return Graphic::GetColor().w; }
		void SetAlpha(float a);
	};

}
