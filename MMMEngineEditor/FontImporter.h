#pragma once

#include "Singleton.hpp"
#include <string>

namespace MMMEngine
{
	class FontImporter : public Utility::Singleton<FontImporter>
	{
	public:
		struct BuildOptions
		{
			int fontSize = 32;
			std::wstring characterRegion; // 예: L"0x20-0x7E"
		};

		bool ConvertTTFToSpriteFont(const std::wstring& inputPath,
			const std::wstring& outputPath,
			const BuildOptions& options,
			std::wstring& outError);

		std::wstring m_exportPath = L"Assets/";
		std::wstring m_makeSpriteFontPath;
		std::wstring m_charsetFilePath;
	};
}
