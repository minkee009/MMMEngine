#include "SFX2DComponent.h"
#include "rttr/registration.h"

RTTR_REGISTRATION
{
	using namespace rttr;
	using namespace MMMEngine;

	registration::class_<SFX2DComponent>("SFX2DComponent")
		(rttr::metadata("wrapper_type_name", "ObjPtr<SFX2DComponent>"));
	registration::class_<ObjPtr<SFX2DComponent>>("ObjPtr<SFX2DComponent>")
		.constructor<>(
			[]() {
				return Object::NewObject<SFX2DComponent>();
			}).method("Inject", &ObjPtr<SFX2DComponent>::Inject);
}


void MMMEngine::SFX2DComponent::UnInitialize()
{
	for (int i = 0; i < 5; i++) {
		if (sfxChannel[i]) {
			sfxChannel[i]->stop();
			sfxChannel[i] = nullptr;
		}
	}
	for (int i = 0; i < 3; i++) {
		if (loopsfxChannel[i]) {
			loopsfxChannel[i]->stop();
			loopsfxChannel[i] = nullptr;
		}
	}
}

void MMMEngine::SFX2DComponent::PlaySFX2D(const std::string& id)
{
	sfxChannel[AcquireSlot()] = AudioManager::Get().PlaySFX2D(id);
}

void MMMEngine::SFX2DComponent::PlayLoopSFX2D(const std::string& id, int slot)
{
	if (slot > 2 || slot < 0)
		return;
	StopLoopSFX2D(slot);
	loopsfxChannel[slot] = AudioManager::Get().PlaySFX2D(id);
}

void MMMEngine::SFX2DComponent::StopLoopSFX2D(int slot)
{
	if (slot > 2 || slot < 0)
		return;
	if (loopsfxChannel[slot]) {
		loopsfxChannel[slot]->stop();
		loopsfxChannel[slot] = nullptr;
	}
}

int MMMEngine::SFX2DComponent::AcquireSlot()
{
	int slot = mNextSlot;
	mNextSlot = (mNextSlot + 1) % 5;

	if (sfxChannel[slot])
	{
		bool playing = false;
		if (sfxChannel[slot]->isPlaying(&playing) != FMOD_OK || !playing)
		{
			sfxChannel[slot] = nullptr;
		}
		else
		{
			sfxChannel[slot]->stop();
			sfxChannel[slot] = nullptr;
		}
	}
	return slot;
}
