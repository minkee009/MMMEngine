#pragma once
#include <string>
#include "rttr/type"
#include "Export.h"

namespace MMMEngine
{
	enum class UnloadState
	{
		ScriptNotLoaded,
		UnloadFail,
		UnloadSuccess
	};
	class MMMENGINE_API ScriptLoader
	{
	private:
		std::unique_ptr<rttr::library> m_pLoadedModule;
	public:
		bool LoadScriptDLL(const std::string& dllName);
		UnloadState UnloadScript();
		~ScriptLoader();
	};
}
