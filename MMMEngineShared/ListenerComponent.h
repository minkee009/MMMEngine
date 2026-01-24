#pragma once
#include "AudioManager.h"
#include "Export.h"
#include "ScriptBehaviour.h"
//#include "Component.h"
#include "SimpleMath.h"

namespace MMMEngine {
	class MMMENGINE_API ListenerComponent : public ScriptBehaviour
	{
	private:
		RTTR_ENABLE(ScriptBehaviour)
		RTTR_REGISTRATION_FRIEND
		friend class ObjectManager;
		friend class GameObject;
		DirectX::SimpleMath::Vector3 pos;
		DirectX::SimpleMath::Vector3 fwd;
		DirectX::SimpleMath::Vector3 up;
	protected:
		ListenerComponent() {};
		virtual void Initialize() override {};
		virtual void UnInitialize() override {};
		void Update();
	};
}