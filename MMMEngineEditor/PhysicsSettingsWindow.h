#pragma once
#include "Singleton.hpp"

namespace MMMEngine::Editor
{
	class PhysicsSettingsWindow : public Utility::Singleton<PhysicsSettingsWindow>
	{
	public:
		void Render();
	};
}
