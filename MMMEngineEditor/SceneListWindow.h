#pragma once
#include <imgui.h>
#include "Singleton.hpp"

namespace MMMEngine::Editor
{
	class SceneListWindow : public Utility::Singleton<SceneListWindow>
	{
	public:
		void RefreshSceneList();
		void Render();
	};
}
