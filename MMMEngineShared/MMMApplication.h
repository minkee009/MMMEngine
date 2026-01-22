#pragma once
#include "GlobalRegistry.h"
#include <cassert>
#include <string>

namespace MMMEngine::Application
{
	inline void Quit() { assert(GlobalRegistry::g_pApp && "글로벌 레지스트리에 Application이 등록되어있지 않습니다!"); GlobalRegistry::g_pApp->Quit(); }

	inline void SetWindowTitle(const std::wstring& title) { assert(GlobalRegistry::g_pApp && "글로벌 레지스트리에 Application이 등록되어있지 않습니다!"); GlobalRegistry::g_pApp->SetWindowTitle(title); }
}