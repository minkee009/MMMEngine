#pragma once
#include "Singleton.hpp"

namespace MMMEngine
{
	class SceneManager : public Singleton<SceneManager>
	{
		void StartUp();
		void ShutDown();
		void Update();
	};
}