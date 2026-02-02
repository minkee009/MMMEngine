#pragma once
#include "Export.h"
#include <SimpleMath.h>

#include "Component.h"
#include "rttr/type"
#include "ShaderInfo.h"

namespace MMMEngine {
	// 라이트 타입
	enum LightType {
		Directional = 0,
		//Point = 1,
	};

	class RenderManager;
	class MMMENGINE_API Light : public Component
	{
		RTTR_ENABLE(Component)
		RTTR_REGISTRATION_FRIEND
		friend class RenderManager;
		
	private:
		// < propertyName, Value >
		std::unordered_map<std::wstring, PropertyValue> m_properties;
		LightType m_lightType;
		int m_lightIndex = -1;
		DirectX::SimpleMath::Vector3 m_lightColor = { 255.0f, 255.0f, 255.0f };
		float m_lightIntensity = 1.0f;

	public:
		const float& GetLightIntensity() { return m_lightIntensity; }
		void SetLightIntensity(const float& _intensity);

		LightType GetLightType() { return m_lightType; }
		void SetLightType(LightType _type) { m_lightType = _type; }

		DirectX::SimpleMath::Vector3& GetLightColor() { return m_lightColor; }
		void SetLightColor(DirectX::SimpleMath::Vector3& _color);
		DirectX::SimpleMath::Vector3 GetNormalizedColor();

		const std::unordered_map<std::wstring, PropertyValue>& GetProperties() { return m_properties; }

		void Initialize() override;
		void UnInitialize() override;

		// 렌더매니저 주도의 업데이트
		void Render();

		bool IsActiveAndEnabled();
	};
}


