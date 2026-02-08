#pragma once
#include "Singleton.hpp"
#include <filesystem>

namespace MMMEngine::Editor
{
	class AudioClipImporterWindow : public Utility::Singleton<AudioClipImporterWindow>
	{
	public:
		void Render();
		void OpenWithPath(const std::filesystem::path& path);
	};
}
