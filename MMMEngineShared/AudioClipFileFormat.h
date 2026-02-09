#pragma once

#include <cstdint>
#include <cstring>

namespace MMMEngine::AudioClipFileFormat
{
	static constexpr uint8_t kMagic[8] = { 'M','M','A','C','L','I','P','\0' };
	static constexpr uint32_t kVersion = 1;

	enum class LoadMode : uint32_t
	{
		Sample = 0,
		Stream = 1,
		StreamNonBlocking = 2
	};

	struct Header
	{
		uint8_t magic[8];
		uint32_t version;
		uint32_t loadMode;
		uint32_t dataSize;
	};

	inline bool HasMagic(const Header& header)
	{
		return std::memcmp(header.magic, kMagic, sizeof(kMagic)) == 0;
	}
}

