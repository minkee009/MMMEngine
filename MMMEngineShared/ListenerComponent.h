#pragma once
#include "AudioManager.h"
#include "Export.h"
#include "Behaviour.h"
//#include "Component.h"
#include "SimpleMath.h"

namespace MMMEngine {
	class MMMENGINE_API ListenerComponent : public Behaviour
	{
	private:
		RTTR_ENABLE(Behaviour)
		RTTR_REGISTRATION_FRIEND
		friend class ObjectManager;
		friend class GameObject;

		DirectX::SimpleMath::Vector3 pos;
		DirectX::SimpleMath::Vector3 fwd;
		DirectX::SimpleMath::Vector3 up;
	protected:
		ListenerComponent() {
			REGISTER_BEHAVIOUR_MESSAGE(Update)
		};
		virtual void Initialize() override {};
		virtual void UnInitialize() override {};
		void Update();
	};
}
