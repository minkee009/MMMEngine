#include "AudioClip.h"

#include "AudioManager.h"
#include "AudioClipFileFormat.h"
#include "ResourceManager.h"
#include "StringHelper.h"

#include <fmod.hpp>
#include <fmod_errors.h>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>

#include <rttr/registration>
#include "json/json.hpp"

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace
{
	std::string ToLowerCopy(std::string s)
	{
		std::transform(s.begin(), s.end(), s.begin(),
			[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		return s;
	}

	FMOD_MODE LoadModeFromString(const std::string& raw)
	{
		std::string lower = ToLowerCopy(raw);
		if (lower == "streamnonblocking" || lower == "stream_nonblocking" || lower == "stream+nonblocking")
			return FMOD_CREATESTREAM | FMOD_NONBLOCKING;
		if (lower == "stream")
			return FMOD_CREATESTREAM;
		return FMOD_DEFAULT;
	}

	FMOD_MODE LoadModeFromHeader(uint32_t rawMode)
	{
		using MMMEngine::AudioClipFileFormat::LoadMode;
		switch (static_cast<LoadMode>(rawMode))
		{
		case LoadMode::StreamNonBlocking:
			return FMOD_CREATESTREAM | FMOD_NONBLOCKING;
		case LoadMode::Stream:
			return FMOD_CREATESTREAM;
		case LoadMode::Sample:
		default:
			return FMOD_DEFAULT;
		}
	}

	bool TryReadEmbeddedAudio(const fs::path& clipPath, std::vector<uint8_t>& outData, FMOD_MODE& outMode)
	{
		outData.clear();
		outMode = FMOD_DEFAULT;

		std::ifstream file(clipPath, std::ios::binary);
		if (!file.is_open())
			return false;

		MMMEngine::AudioClipFileFormat::Header header{};
		file.read(reinterpret_cast<char*>(&header), sizeof(header));
		if (!file.good())
			return false;

		if (!MMMEngine::AudioClipFileFormat::HasMagic(header))
			return false;

		if (header.version != MMMEngine::AudioClipFileFormat::kVersion)
			return false;

		if (header.dataSize == 0)
			return false;

		outData.resize(header.dataSize);
		file.read(reinterpret_cast<char*>(outData.data()), header.dataSize);
		if (!file.good())
		{
			outData.clear();
			return false;
		}

		outMode = LoadModeFromHeader(header.loadMode);
		return true;
	}

	bool ResolveAudioSourcePath(const fs::path& clipPath, const std::string& source, fs::path& outPath)
	{
		if (source.empty())
			return false;

		fs::path sourcePath(source);
		if (sourcePath.is_absolute())
		{
			outPath = sourcePath;
			return true;
		}

		fs::path root = MMMEngine::ResourceManager::Get().GetCurrentRootPath();
		fs::path candidate = root / sourcePath;
		if (fs::exists(candidate))
		{
			outPath = candidate;
			return true;
		}

		fs::path relativeToClip = clipPath.parent_path() / sourcePath;
		if (fs::exists(relativeToClip))
		{
			outPath = relativeToClip;
			return true;
		}

		return false;
	}
}

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
	m_embeddedData.clear();
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
	m_embeddedData.clear();

	auto* system = AudioManager::Get().GetSystem();
	if (!system)
		return false;

	fs::path audioPath = fPath;
	FMOD_MODE mode = FMOD_DEFAULT;

	std::string extLower = ToLowerCopy(fPath.extension().string());
	if (extLower == ".audioclip")
	{
		std::vector<uint8_t> embedded;
		FMOD_MODE embeddedMode = FMOD_DEFAULT;
		if (TryReadEmbeddedAudio(fPath, embedded, embeddedMode))
		{
			if (embedded.size() > std::numeric_limits<unsigned int>::max())
			{
				std::cout << "AudioClip::Embedded audio too large." << std::endl;
				return false;
			}

			m_embeddedData = std::move(embedded);
			FMOD_CREATESOUNDEXINFO exinfo{};
			exinfo.cbsize = sizeof(exinfo);
			exinfo.length = static_cast<unsigned int>(m_embeddedData.size());

			FMOD_MODE finalMode = embeddedMode | FMOD_OPENMEMORY;
			FMOD_RESULT memResult = system->createSound(
				reinterpret_cast<const char*>(m_embeddedData.data()),
				finalMode,
				&exinfo,
				&m_sound);
			if (memResult != FMOD_OK && (embeddedMode & FMOD_CREATESTREAM))
			{
				finalMode = FMOD_OPENMEMORY;
				memResult = system->createSound(
					reinterpret_cast<const char*>(m_embeddedData.data()),
					finalMode,
					&exinfo,
					&m_sound);
			}
			if (memResult != FMOD_OK)
			{
				std::cout << "[FMOD] createSound(memory) failed: " << FMOD_ErrorString(memResult) << std::endl;
				m_sound = nullptr;
				m_embeddedData.clear();
				return false;
			}
			return true;
		}

		std::ifstream file(fPath);
		if (!file.is_open())
		{
			std::cout << "AudioClip::Failed to open audioclip file." << std::endl;
			return false;
		}

		json data = json::parse(file, nullptr, false);
		if (data.is_discarded() || !data.is_object())
		{
			std::cout << "AudioClip::Invalid audioclip json." << std::endl;
			return false;
		}

		std::string source = data.value("source", "");
		std::string loadMode = data.value("loadMode", "Sample");
		mode = LoadModeFromString(loadMode);

		fs::path resolved;
		if (!ResolveAudioSourcePath(fPath, source, resolved))
		{
			std::cout << "AudioClip::Audio source not found." << std::endl;
			return false;
		}
		audioPath = resolved;
	}

	const std::string path = Utility::StringHelper::WStringToString(audioPath.wstring());
	FMOD_RESULT result = system->createSound(path.c_str(), mode, nullptr, &m_sound);
	if (result != FMOD_OK)
	{
		std::cout << "[FMOD] createSound failed: " << FMOD_ErrorString(result) << std::endl;
		m_sound = nullptr;
		return false;
	}

	return true;
}
