#include "BGMComponent.h"
#include "rttr/registration.h"

RTTR_REGISTRATION
{
	using namespace rttr;
	using namespace MMMEngine;

	registration::class_<BGMComponent>("BGMComponent")
		(rttr::metadata("wrapper_type_name", "ObjPtr<BGMComponent>"));
	registration::class_<ObjPtr<BGMComponent>>("ObjPtr<BGMComponent>")
		.constructor<>(
			[]() {
				return Object::NewObject<BGMComponent>();
			}).method("Inject", &ObjPtr<BGMComponent>::Inject);
}

void MMMEngine::BGMComponent::PlayBGM(const std::string& id)
{
	StopSound();
	bgmChannel = AudioManager::Get().PlayBGM(id);
}

void MMMEngine::BGMComponent::StopSound()
{
	if (bgmChannel)
	{
		bgmChannel->stop();
		bgmChannel = nullptr;
	}
}

void MMMEngine::BGMComponent::UnInitialize()
{
	StopSound();
}
