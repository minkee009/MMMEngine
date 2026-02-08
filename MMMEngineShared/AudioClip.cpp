#include "AudioClip.h"

#include "AudioManager.h"
#include "StringHelper.h"

#include <fmod.hpp>
#include <fmod_errors.h>
#include <filesystem>
#include <iostream>

#include <rttr/registration>

namespace fs = std::filesystem;

RTTR_REGISTRATION
{
	using namespace rttr;
	using namespace MMMEngine;

	registration::class_<AudioClip>("AudioClip")
		.constructor<>()(policy::ctor::as_std_shared_ptr)
		.property_readonly("GetFilePath", &AudioClip::GetFilePath);

	type::register_converter_func(
		[](std::shared_ptr<Resource> from, bool& ok) -> std::shared_ptr<AudioClip>
		{
			if (!from)
			{
				ok = true;
				return nullptr;
			}
			auto result = std::dynamic_pointer_cast<AudioClip>(from);
			ok = (result != nullptr);
			return result;
		}
	);
}

MMMEngine::AudioClip::~AudioClip()
{
	if (m_sound)
	{
		m_sound->release();
		m_sound = nullptr;
	}
}

bool MMMEngine::AudioClip::LoadFromFilePath(const std::wstring& filePath)
{
	if (!AudioManager::Get().IsInitialized())
		return false;

	fs::path fPath(filePath);
	if (!fs::exists(fPath))
	{
		std::cout << "AudioClip::File does not exist." << std::endl;
		return false;
	}

	if (m_sound)
	{
		m_sound->release();
		m_sound = nullptr;
	}

	auto* system = AudioManager::Get().GetSystem();
	if (!system)
		return false;

	const std::string path = Utility::StringHelper::WStringToString(filePath);
	FMOD_RESULT result = system->createSound(path.c_str(), FMOD_DEFAULT, nullptr, &m_sound);
	if (result != FMOD_OK)
	{
		std::cout << "[FMOD] createSound failed: " << FMOD_ErrorString(result) << std::endl;
		m_sound = nullptr;
		return false;
	}

	return true;
}
