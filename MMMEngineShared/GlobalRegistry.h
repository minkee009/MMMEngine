#pragma once
#include "Export.h"

namespace MMMEngine::Utility
{
	class App;
}

namespace MMMEngine::GlobalRegistry
{
	extern MMMENGINE_API Utility::App* g_pApp;
	extern MMMENGINE_API bool g_runtimeActive;
	extern MMMENGINE_API bool g_quitRequested;
}
