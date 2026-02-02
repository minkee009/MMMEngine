#pragma once
#include "Component.h"
#include "rttr/type"

namespace MMMEngine {
	class RenderManager;
	class MMMENGINE_API Renderer : public Component
	{
		RTTR_ENABLE(Component)
		RTTR_REGISTRATION_FRIEND
		friend class RenderManager;
	protected:
		uint32_t renderIndex = UINT32_MAX;
		bool isEnabled = true;
		bool castShadows = true;
		bool receiveShadows = true;

		virtual void Render() {}
		virtual ~Renderer() {}
		virtual void Initialize() override {};
		virtual void UnInitialize() override {};

	public:
		bool GetEnabled() { return isEnabled; }
		void SetEnabled(bool _val) { isEnabled = _val; }
		uint32_t GetRenderIndex() const { return renderIndex; }

		bool IsActiveAndEnabled();
	};
}


