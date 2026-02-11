#pragma once
#include "SceneManager.h"

namespace MMMEngine::SceneManagement
{
	inline void ChangeScene(const std::string& name) { SceneManager::Get().ChangeScene(name); }
	inline void ChangeScene(const size_t& id) { SceneManager::Get().ChangeScene(id); }
}
