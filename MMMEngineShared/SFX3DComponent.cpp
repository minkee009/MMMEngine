#include "SFX3DComponent.h"
#include "Transform.h"
#include "rttr/registration.h"

RTTR_REGISTRATION
{
	using namespace rttr;
	using namespace MMMEngine;

	registration::class_<SFX3DComponent>("SFX3DComponent")
		(rttr::metadata("wrapper_type_name", "ObjPtr<SFX3DComponent>"));
	registration::class_<ObjPtr<SFX3DComponent>>("ObjPtr<SFX3DComponent>")
		.constructor<>(
			[]() {
				return Object::NewObject<SFX3DComponent>();
			}).method("Inject", &ObjPtr<SFX3DComponent>::Inject);
}


void MMMEngine::SFX3DComponent::Initialize()
{

}

void MMMEngine::SFX3DComponent::UnInitialize()
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

void MMMEngine::SFX3DComponent::Update()
{
	auto transform = GetTransform();
	x = transform->GetWorldPosition().x;
	y = transform->GetWorldPosition().y;
	z = transform->GetWorldPosition().z;
	FMOD_VECTOR pos = V3(x, y, z);
	FMOD_VECTOR vel = V3(0.0f, 0.0f, 0.0f);
	for (int i = 0; i < 5; i++)
	{
		FMOD::Channel* ch = sfxChannel[i];
		if (!ch) continue;

		bool playing = false;
		if (ch->isPlaying(&playing) != FMOD_OK || !playing)
		{
			sfxChannel[i] = nullptr;
			continue;
		}
		ch->set3DAttributes(&pos, &vel);
	}
}

void MMMEngine::SFX3DComponent::PlaySFX3D(const std::string& id)
{
	sfxChannel[AcquireSlot()] = AudioManager::Get().PlaySFX3D(id, x, y, z);
}

void MMMEngine::SFX3DComponent::PlayLoopSFX3D(const std::string& id, int slot)
{
	if (slot > 2 || slot < 0)
		return;
	StopLoopSFX3D(slot);
	loopsfxChannel[slot] = AudioManager::Get().PlaySFX3D(id, x, y, z);
}

void MMMEngine::SFX3DComponent::StopLoopSFX3D(int slot)
{
	if (slot > 2 || slot < 0)
		return;
	if (loopsfxChannel[slot]) {
		loopsfxChannel[slot]->stop();
		loopsfxChannel[slot] = nullptr;
	}
}

int MMMEngine::SFX3DComponent::AcquireSlot()
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
