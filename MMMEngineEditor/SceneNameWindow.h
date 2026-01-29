#pragma once
#include <imgui.h>
#include "Singleton.hpp"

namespace MMMEngine::Editor
{
	class SceneNameWindow : public Utility::Singleton<SceneNameWindow>
	{
	private:
		bool m_hasChanges = false;
		bool m_firstShowSceneName = true;
	public:
		void Render();
	};
}
