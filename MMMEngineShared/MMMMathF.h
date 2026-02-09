#pragma once
#include "Export.h"
#include "MMMTime.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace MMMEngine
{
	class MMMENGINE_API MathF
	{
	public:
		static constexpr float Infinity = std::numeric_limits<float>::infinity();

		static float Clamp01(float value);

		static float Clamp(float value, float min, float max);

		static float SmoothDamp(float current, float target, float& currentVelocity, float smoothTime,
			float maxSpeed = Infinity, float deltaTime = Time::GetDeltaTime());

		static float Lerp(float a, float b, float t);

		static float Max(float a, float b);

		static float Min(float a, float b);

		static float Sin(float f);

		static float Sign(float f);

		static float PerlinNoise(float x, float y);

		static float PerlinNoise1D(float x);

		static bool Approximately(float a, float b);

		static float Round(float f);

		static int RoundToInt(float f);

	private:
		MathF() = delete;

		static float Fade(float t);

		static float Grad(int hash, float x, float y);

		static float Grad1D(int hash, float x);

		static const int* GetPermutation();
	};
}
