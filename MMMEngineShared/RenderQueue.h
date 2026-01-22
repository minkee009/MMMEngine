#pragma once

namespace MMMEngine
{
	enum class RenderQueue : int {
		Shadow = 0,
		SkyBox = 500,
		Opaque = 1000,
		AlphaTest = 1500,
		Transparent = 2000,
		Particle = 3000,
		PostProcess = 4000,
		UI = 5000
	};
}