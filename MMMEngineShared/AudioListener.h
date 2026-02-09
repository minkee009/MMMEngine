#pragma once

#include "Behaviour.h"
#include "Export.h"
#include "SimpleMath.h"

struct FMOD_VECTOR;

namespace MMMEngine
{
	class MMMENGINE_API AudioListener : public Behaviour
	{
	private:
		RTTR_ENABLE(Behaviour)
		RTTR_REGISTRATION_FRIEND
		friend class AudioManager;

		bool m_isMain = false;
		DirectX::SimpleMath::Vector3 m_lastPosition = DirectX::SimpleMath::Vector3::Zero;
		bool m_hasLastPosition = false;

		void SetMainFlag(bool value);

	public:
		AudioListener() = default;

		void Initialize() override;
		void UnInitialize() override;

		void SetAsMainListener();
		bool IsMainListener() const { return m_isMain; }

		void BuildAttributes(float dt, FMOD_VECTOR& outPos, FMOD_VECTOR& outVel, FMOD_VECTOR& outForward, FMOD_VECTOR& outUp);
	};
}
