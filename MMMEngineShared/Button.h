#pragma once

#include "Export.h"
#include "Graphic.h"
#include "SerializableEvent.h"
#include "SimpleMath.h"

namespace MMMEngine
{
	/// 버튼 UI. Graphic 직접 상속. 호버/클릭 이벤트 + Graphic의 Color/Texture로 그리기.
	class MMMENGINE_API Button : public Graphic
	{
	private:
		RTTR_ENABLE(Graphic)
		RTTR_REGISTRATION_FRIEND

		bool m_isHovered = false;
		bool m_isPressed = false;

		SerializableEvent m_onHoverEnter;
		SerializableEvent m_onHoverStay;
		SerializableEvent m_onHoverExit;
		SerializableEvent m_onClick;
		SerializableEvent m_onRelease;

	public:
		Button() = default;
		virtual ~Button() = default;

		const SerializableEvent& GetOnHoverEnter() const { return m_onHoverEnter; }
		void SetOnHoverEnter(const SerializableEvent& ev) { m_onHoverEnter = ev; }
		SerializableEvent& OnHoverEnter() { return m_onHoverEnter; }

		const SerializableEvent& GetOnHoverStay() const { return m_onHoverStay; }
		void SetOnHoverStay(const SerializableEvent& ev) { m_onHoverStay = ev; }
		SerializableEvent& OnHoverStay() { return m_onHoverStay; }

		const SerializableEvent& GetOnHoverExit() const { return m_onHoverExit; }
		void SetOnHoverExit(const SerializableEvent& ev) { m_onHoverExit = ev; }
		SerializableEvent& OnHoverExit() { return m_onHoverExit; }

		const SerializableEvent& GetOnClick() const { return m_onClick; }
		void SetOnClick(const SerializableEvent& ev) { m_onClick = ev; }
		SerializableEvent& OnClick() { return m_onClick; }

		const SerializableEvent& GetOnRelease() const { return m_onRelease; }
		void SetOnRelease(const SerializableEvent& ev) { m_onRelease = ev; }
		SerializableEvent& OnRelease() { return m_onRelease; }

		bool IsHovered() const { return m_isHovered; }
		bool IsPressed() const { return m_isPressed; }

		void UpdatePointer(const DirectX::SimpleMath::Vector2& canvasSize,
			const DirectX::SimpleMath::Vector2& pointerInCanvas,
			bool isMouseDown);
	};
}
