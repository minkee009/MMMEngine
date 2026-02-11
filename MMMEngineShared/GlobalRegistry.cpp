#include "GlobalRegistry.h"
#include "App.h"

MMMEngine::Utility::App* MMMEngine::GlobalRegistry::g_pApp = nullptr;
bool MMMEngine::GlobalRegistry::g_runtimeActive = false;
bool MMMEngine::GlobalRegistry::g_quitRequested = false;
