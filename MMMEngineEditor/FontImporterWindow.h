#pragma once
#include "Singleton.hpp"

namespace MMMEngine::Editor
{
	class FontImporterWindow : public Utility::Singleton<FontImporterWindow>
	{
	public:
		void Render();
	};
}
