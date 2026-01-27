#pragma once

#include <filesystem>
#include <chrono>
#include <thread>

namespace fs = std::filesystem;

namespace MMMEngine::Editor
{
	class DLLHotLoadHelper 
	{
	public:
        static std::wstring MakeHotDllName(const fs::path& originalDllPath);
		static fs::path CopyDllForHotReload(const fs::path& dllPath, const fs::path& hotDir);
		static void DLLHotLoadHelper::PrepareForBuild(const fs::path& binDir);
		static void CleanupHotReloadCopies(const fs::path& hotDir);
		static void BackupOriginalDll(const fs::path& binDir);
		static void RestoreOriginalDll(const fs::path& binDir);
	};
}
