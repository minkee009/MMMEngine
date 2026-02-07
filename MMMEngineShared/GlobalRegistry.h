#pragma once
#include "Export.h"

namespace MMMEngine::Utility
{
	class App;
}

namespace MMMEngine::GlobalRegistry
{
	extern MMMENGINE_API Utility::App* g_pApp;
	inline bool g_runtimeActive = false;
}
