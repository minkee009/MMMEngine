#pragma once
#include <cstdint>

namespace MMMEngine
{
	struct SceneRef
	{
		size_t id = static_cast<size_t>(-1);
		bool id_DDOL = false; //is Dont Destroy On Load;
	};
}