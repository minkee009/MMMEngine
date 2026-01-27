#pragma once
#include "ExportSingleton.hpp"
#include <filesystem>

namespace MMMEngine
{
	class PhysicsSettings : public Utility::ExportSingleton<PhysicsSettings>
	{
	public:
		void SaveSettings(std::filesystem::path filePath);
		void LoadSettings(std::filesystem::path filePath);
	private:
		std::filesystem::path m_settingsPath;
	};
}
