#pragma once

#include "Export.h"
#include "Behaviour.h"
#include "ResourceManager.h"
#include "SimpleMath.h"

namespace MMMEngine
{
	class Canvas;
	class RectTransform;
	class RenderManager;
	class Texture2D;

	class MMMENGINE_API Graphic : public Behaviour
	{
	private:
		RTTR_ENABLE(Behaviour)
		RTTR_REGISTRATION_FRIEND

		ObjPtr<Canvas> m_canvas;
		DirectX::SimpleMath::Color m_color = { 1.0f,1.0f,1.0f,1.0f };
		ResPtr<Texture2D> m_texture = nullptr;
		int m_renderOrder = 0;
		int m_cachedEffectiveRenderOrder = 0;
		int m_cachedRenderOrderValue = 0;
		int m_cachedParentEffectiveOrder = 0;
		bool m_hadCachedParentGraphic = false;
		ObjPtr<Graphic> m_cachedParentGraphic;
		bool m_effectiveOrderDirty = true;

		void RefreshCanvas(ObjPtr<Transform> newParent);
		void HandleTransformParentChanged(ObjPtr<Transform> newParent);
		void MarkEffectiveOrderDirty();
		void MarkEffectiveOrderDirtyRecursive();
		int RecalculateEffectiveRenderOrder();

	protected:
		void Initialize() override;
		void UnInitialize() override;
		void ApplyNativeSizeFromTexture(const ResPtr<Texture2D>& texture);

	public:
		virtual ~Graphic() = default;
		bool RequiresRectTransform() const override { return true; }

		const DirectX::SimpleMath::Color& GetColor() const { return m_color; }
		void SetColor(const DirectX::SimpleMath::Color& color) { m_color = color; }

		const ResPtr<Texture2D>& GetTexture() { return m_texture; }
		void SetTexture(const ResPtr<Texture2D>& texture) { m_texture = texture; }

		int GetRenderOrder() const { return m_renderOrder; }
		void SetRenderOrder(int order)
		{
			if (m_renderOrder == order)
				return;
			m_renderOrder = order;
			MarkEffectiveOrderDirtyRecursive();
		}
		int GetEffectiveRenderOrder();

		ObjPtr<Canvas> GetCanvas() const { return m_canvas; }
		ObjPtr<RectTransform> GetRectTransform();
		void RefreshCanvasNow();

		virtual DirectX::SimpleMath::Vector4 GetUVRect() const { return { 0.0f, 0.0f, 1.0f, 1.0f }; }
		virtual void RenderUI(RenderManager& renderer);
	};
}
