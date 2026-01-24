#pragma once
#include "AudioManager.h"
#include "Component.h"
#include "Export.h"

namespace MMMEngine {
	class MMMENGINE_API SFX2DComponent : public Component
	{
	private:
		RTTR_ENABLE(Component)
		RTTR_REGISTRATION_FRIEND
		friend class ObjectManager;
		friend class GameObject;
		FMOD::Channel* sfxChannel[5] = {};
		FMOD::Channel* loopsfxChannel[3] = {};
		int mNextSlot = 0;

	protected:
		SFX2DComponent() {};
		virtual void Initialize() override {};
		virtual void UnInitialize() override;
		int AcquireSlot();
	public:
		void PlaySFX2D(const std::string& id);
		void PlayLoopSFX2D(const std::string& id, int slot);
		void StopLoopSFX2D(int slot);
	};
}

