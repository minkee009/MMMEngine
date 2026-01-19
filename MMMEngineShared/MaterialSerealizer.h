#pragma once
#include "ExportSingleton.hpp"
#include <string>

namespace MMMEngine {
	class MMMENGINE_API MaterialSerealizer : public Utility::ExportSingleton<MaterialSerealizer>
	{
	public:
		bool Serealize(std::wstring _path);
	};
}


