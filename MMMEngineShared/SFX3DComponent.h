#pragma once
#include "AudioManager.h"
#include "Export.h"
#include "Behaviour.h"
//#include "Component.h"
#include "Transform.h"

namespace MMMEngine {
	class MMMENGINE_API SFX3DComponent : public Behaviour
	{
	private:
		RTTR_ENABLE(Behaviour)
		RTTR_REGISTRATION_FRIEND
		friend class ObjectManager;
		friend class GameObject;

		FMOD::Channel* sfxChannel[5] = {};
		FMOD::Channel* loopsfxChannel[3] = {};
		int mNextSlot = 0;
		float x = 0.0f, y = 0.0f, z = 0.f;
	protected:
		SFX3DComponent() 
		{
			REGISTER_BEHAVIOUR_MESSAGE(Update)
		};
		virtual void Initialize() override;
		virtual void UnInitialize() override;
		void Update();
		int AcquireSlot();
	public:
		void PlaySFX3D(const std::string& id);
		void PlayLoopSFX3D(const std::string& id, int slot);
		void StopLoopSFX3D(int slot);
	};
}

