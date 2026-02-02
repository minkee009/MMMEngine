#pragma once

#include "Export.h"
#include "Graphic.h"
#include "Gage.h"
#include "ResourceManager.h"
#include "SerializableEvent.h"
#include "SimpleMath.h"

namespace MMMEngine
{
	class Texture2D;

	/// 핸들 게이지 UI. Graphic 직접 상속. 드래그로 value 조절 + OnValueChanged.
	class MMMENGINE_API HandleGage : public Graphic
	{
	private:
		RTTR_ENABLE(Graphic)
		RTTR_REGISTRATION_FRIEND

		ResPtr<Texture2D> m_backgroundTexture = nullptr;
		ResPtr<Texture2D> m_fillTexture = nullptr;
		ResPtr<Texture2D> m_handleTexture = nullptr;
		DirectX::SimpleMath::Vector2 m_handleSize = { 0.0f, 0.0f };
		DirectX::SimpleMath::Vector2 m_handleOffset = { 0.0f, 0.0f };
		DirectX::SimpleMath::Vector2 m_handlePivot = { 0.5f, 0.5f };
		DirectX::SimpleMath::Vector2 m_handleClampPadding = { 0.0f, 0.0f };
		bool m_hideHandleWhenZero = false;
		float m_value = 1.0f;
		GageFillDirection m_fillDirection = GageFillDirection::LeftToRight;
		bool m_isDragging = false;
		SerializableEventT<float> m_onValueChanged;

	public:
		HandleGage() = default;
		virtual ~HandleGage() = default;

		const ResPtr<Texture2D>& GetBackgroundTexture() const { return m_backgroundTexture; }
		void SetBackgroundTexture(const ResPtr<Texture2D>& tex) { m_backgroundTexture = tex; }

		const ResPtr<Texture2D>& GetFillTexture() const { return m_fillTexture; }
		void SetFillTexture(const ResPtr<Texture2D>& tex) { m_fillTexture = tex; }

		const ResPtr<Texture2D>& GetHandleTexture() const { return m_handleTexture; }
		void SetHandleTexture(const ResPtr<Texture2D>& tex) { m_handleTexture = tex; }

		const DirectX::SimpleMath::Vector2& GetHandleSize() const { return m_handleSize; }
		void SetHandleSize(const DirectX::SimpleMath::Vector2& size) { m_handleSize = size; }

		const DirectX::SimpleMath::Vector2& GetHandleOffset() const { return m_handleOffset; }
		void SetHandleOffset(const DirectX::SimpleMath::Vector2& offset) { m_handleOffset = offset; }

		const DirectX::SimpleMath::Vector2& GetHandlePivot() const { return m_handlePivot; }
		void SetHandlePivot(const DirectX::SimpleMath::Vector2& pivot) { m_handlePivot = pivot; }

		const DirectX::SimpleMath::Vector2& GetHandleClampPadding() const { return m_handleClampPadding; }
		void SetHandleClampPadding(const DirectX::SimpleMath::Vector2& padding) { m_handleClampPadding = padding; }

		bool GetHideHandleWhenZero() const { return m_hideHandleWhenZero; }
		void SetHideHandleWhenZero(bool hide) { m_hideHandleWhenZero = hide; }

		float GetValue() const { return m_value; }
		void SetValue(float v);

		GageFillDirection GetFillDirection() const { return m_fillDirection; }
		void SetFillDirection(GageFillDirection dir) { m_fillDirection = dir; }

		const SerializableEventT<float>& GetOnValueChanged() const { return m_onValueChanged; }
		void SetOnValueChanged(const SerializableEventT<float>& ev) { m_onValueChanged = ev; }
		SerializableEventT<float>& OnValueChanged() { return m_onValueChanged; }

		void RenderUI(RenderManager& renderer) override;

		void UpdatePointer(const DirectX::SimpleMath::Vector2& canvasSize,
			const DirectX::SimpleMath::Vector2& pointerInCanvas,
			bool isMouseDown);
	};
}
