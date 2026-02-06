#pragma once

#include "Export.h"
#include "Behaviour.h"

namespace MMMEngine
{
	class Graphic;

	/// UI 마스킹 컴포넌트. 같은 GameObject의 Graphic을 마스크로 사용.
	class MMMENGINE_API UIMask : public Behaviour
	{
	private:
		RTTR_ENABLE(Behaviour)
		RTTR_REGISTRATION_FRIEND

		float m_alphaThreshold = 0.001f;
		bool m_showGraphic = true;

	public:
		UIMask() = default;
		virtual ~UIMask() = default;

		bool RequiresRectTransform() const override { return true; }

		float GetAlphaThreshold() const { return m_alphaThreshold; }
		void SetAlphaThreshold(float v);

		bool GetShowGraphic() const { return m_showGraphic; }
		void SetShowGraphic(bool show) { m_showGraphic = show; }

		ObjPtr<Graphic> GetTargetGraphic() const;
	};
}
