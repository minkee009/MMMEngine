#pragma once

#include "Export.h"
#include "Transform.h"
#include "SimpleMath.h"
#include "rttr/type"

namespace MMMEngine
{
	class MMMENGINE_API RectTransform : public Transform
	{
	private:
		RTTR_ENABLE(Transform)
		RTTR_REGISTRATION_FRIEND

		DirectX::SimpleMath::Vector2 m_anchorMin = { 0.5f, 0.5f };
		DirectX::SimpleMath::Vector2 m_anchorMax = { 0.5f, 0.5f };
		DirectX::SimpleMath::Vector2 m_pivot = { 0.5f, 0.5f };
		DirectX::SimpleMath::Vector2 m_sizeDelta = { 100.0f, 100.0f };

	public:
		RectTransform();
		virtual ~RectTransform() = default;

		DirectX::SimpleMath::Vector2 GetAnchoredPosition() const;
		void SetAnchoredPosition(DirectX::SimpleMath::Vector2 pos);

		const DirectX::SimpleMath::Vector2& GetAnchorMin() const { return m_anchorMin; }
		const DirectX::SimpleMath::Vector2& GetAnchorMax() const { return m_anchorMax; }
		const DirectX::SimpleMath::Vector2& GetPivot() const { return m_pivot; }
		const DirectX::SimpleMath::Vector2& GetSizeDelta() const { return m_sizeDelta; }

		void SetAnchorMin(const DirectX::SimpleMath::Vector2& v);
		void SetAnchorMax(const DirectX::SimpleMath::Vector2& v);
		void SetPivot(const DirectX::SimpleMath::Vector2& v);
		void SetSizeDelta(const DirectX::SimpleMath::Vector2& v);

		float GetWidth() const { return m_sizeDelta.x; }
		float GetHeight() const { return m_sizeDelta.y; }
		void SetWidth(float w);
		void SetHeight(float h);

		// Returns rect in canvas space (x, y, width, height) using pixel coordinates.
		DirectX::SimpleMath::Vector4 GetRectInCanvas(const DirectX::SimpleMath::Vector2& canvasSize) const;
		// Returns pivot position in canvas space (pixel coordinates).
		DirectX::SimpleMath::Vector2 GetPivotPositionInCanvas(const DirectX::SimpleMath::Vector2& canvasSize) const;
		// Returns anchored (pivot) position in canvas space (pixel coordinates).
		DirectX::SimpleMath::Vector2 GetAnchoredPositionInCanvas(const DirectX::SimpleMath::Vector2& canvasSize) const;

		// Anchor helpers (parent rect in canvas space)
		void GetAnchorData(const DirectX::SimpleMath::Vector2& canvasSize,
			DirectX::SimpleMath::Vector2& anchorCenter,
			DirectX::SimpleMath::Vector2& anchorSpan) const;
		DirectX::SimpleMath::Vector4 GetAnchorRectInCanvas(
			const DirectX::SimpleMath::Vector2& canvasSize) const;
	};
}
