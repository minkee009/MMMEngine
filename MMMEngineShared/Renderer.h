#pragma once
#include "Export.h"
#include "Component.h"

namespace MMMEngine
{
	class MMMENGINE_API Renderer : public Component
	{
	private:
		friend class Camera;
		friend class RenderManager;
		bool m_enabled = true;
		int m_renderPriority = 0;

	protected:
		virtual void FillRenderCommand(ObjPtr<Camera> camera) = 0;
	public:
		Renderer() = default;
		~Renderer() = default;

		virtual void Initialize() override;
		virtual void UnInitialize() override;

		bool GetEnabled();
		void SetEnabled(bool enabled);
		int GetRenderPriority();
		void SetRenderPriority(int priority);
	};
}