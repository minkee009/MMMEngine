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

		static float Clamp01(float value)
		{
			if (value < 0.0f)
			{
				return 0.0f;
			}
			if (value > 1.0f)
			{
				return 1.0f;
			}
			return value;
		}

		static float Clamp(float value, float min, float max)
		{
			if (value < min)
			{
				return min;
			}
			if (value > max)
			{
				return max;
			}
			return value;
		}

		static float SmoothDamp(float current, float target, float& currentVelocity, float smoothTime,
			float maxSpeed = Infinity, float deltaTime = Time::GetDeltaTime())
		{
			smoothTime = std::max(0.0001f, smoothTime);
			const float omega = 2.0f / smoothTime;

			const float x = omega * deltaTime;
			const float exp = 1.0f / (1.0f + x + 0.48f * x * x + 0.235f * x * x * x);
			float change = current - target;
			const float originalTo = target;

			const float maxChange = maxSpeed * smoothTime;
			change = Clamp(change, -maxChange, maxChange);
			target = current - change;

			const float temp = (currentVelocity + omega * change) * deltaTime;
			currentVelocity = (currentVelocity - omega * temp) * exp;
			float output = target + (change + temp) * exp;

			if ((originalTo - current > 0.0f) == (output > originalTo))
			{
				output = originalTo;
				currentVelocity = (deltaTime > 0.0f) ? (output - originalTo) / deltaTime : 0.0f;
			}

			return output;
		}

		static float Lerp(float a, float b, float t)
		{
			t = Clamp01(t);
			return a + (b - a) * t;
		}

		static float Max(float a, float b)
		{
			return (a > b) ? a : b;
		}

		static float Min(float a, float b)
		{
			return (a < b) ? a : b;
		}

		static float Sin(float f)
		{
			return std::sin(f);
		}

		static float Sign(float f)
		{
			if (f > 0.0f)
			{
				return 1.0f;
			}
			if (f < 0.0f)
			{
				return -1.0f;
			}
			return 0.0f;
		}

		static float PerlinNoise(float x, float y)
		{
			const int* perm = GetPermutation();
			const float xf = x - std::floor(x);
			const float yf = y - std::floor(y);
			const int xi = static_cast<int>(std::floor(x)) & 255;
			const int yi = static_cast<int>(std::floor(y)) & 255;

			const float u = Fade(xf);
			const float v = Fade(yf);

			const int aa = perm[(perm[xi] + yi) & 255];
			const int ab = perm[(perm[xi] + yi + 1) & 255];
			const int ba = perm[(perm[(xi + 1) & 255] + yi) & 255];
			const int bb = perm[(perm[(xi + 1) & 255] + yi + 1) & 255];

			const float x1 = Lerp(Grad(aa, xf, yf), Grad(ba, xf - 1.0f, yf), u);
			const float x2 = Lerp(Grad(ab, xf, yf - 1.0f), Grad(bb, xf - 1.0f, yf - 1.0f), u);

			const float value = Lerp(x1, x2, v);
			return Clamp01((value + 1.0f) * 0.5f);
		}

		static float PerlinNoise1D(float x)
		{
			const int* perm = GetPermutation();
			const float xf = x - std::floor(x);
			const int xi = static_cast<int>(std::floor(x)) & 255;

			const float u = Fade(xf);
			const int a = perm[xi];
			const int b = perm[(xi + 1) & 255];

			const float value = Lerp(Grad1D(a, xf), Grad1D(b, xf - 1.0f), u);
			return Clamp01((value + 1.0f) * 0.5f);
		}

		static bool Approximately(float a, float b)
		{
			const float diff = std::fabs(a - b);
			const float maxAbs = std::max(std::fabs(a), std::fabs(b));
			return diff <= std::max(1.0e-6f * maxAbs, std::numeric_limits<float>::epsilon() * 8.0f);
		}

		static float Round(float f)
		{
			return std::round(f);
		}

		static int RoundToInt(float f)
		{
			return static_cast<int>(std::round(f));
		}

	private:
		MathF() = delete;

		static float Fade(float t)
		{
			return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
		}

		static float Grad(int hash, float x, float y)
		{
			const int h = hash & 7;
			const float invSqrt2 = 0.70710678f;

			switch (h)
			{
			case 0: return (x + y) * invSqrt2;
			case 1: return (-x + y) * invSqrt2;
			case 2: return (x - y) * invSqrt2;
			case 3: return (-x - y) * invSqrt2;
			case 4: return x;
			case 5: return -x;
			case 6: return y;
			default: return -y;
			}
		}

		static float Grad1D(int hash, float x)
		{
			return (hash & 1) ? -x : x;
		}

		static const int* GetPermutation()
		{
			static const int kPermutation[256] = {
				151, 160, 137, 91, 90, 15, 131, 13, 201, 95, 96, 53, 194, 233, 7, 225,
				140, 36, 103, 30, 69, 142, 8, 99, 37, 240, 21, 10, 23, 190, 6, 148,
				247, 120, 234, 75, 0, 26, 197, 62, 94, 252, 219, 203, 117, 35, 11, 32,
				57, 177, 33, 88, 237, 149, 56, 87, 174, 20, 125, 136, 171, 168, 68, 175,
				74, 165, 71, 134, 139, 48, 27, 166, 77, 146, 158, 231, 83, 111, 229, 122,
				60, 211, 133, 230, 220, 105, 92, 41, 55, 46, 245, 40, 244, 102, 143, 54,
				65, 25, 63, 161, 1, 216, 80, 73, 209, 76, 132, 187, 208, 89, 18, 169,
				200, 196, 135, 130, 116, 188, 159, 86, 164, 100, 109, 198, 173, 186, 3, 64,
				52, 217, 226, 250, 124, 123, 5, 202, 38, 147, 118, 126, 255, 82, 85, 212,
				207, 206, 59, 227, 47, 16, 58, 17, 182, 189, 28, 42, 223, 183, 170, 213,
				119, 248, 152, 2, 44, 154, 163, 70, 221, 153, 101, 155, 167, 43, 172, 9,
				129, 22, 39, 253, 19, 98, 108, 110, 79, 113, 224, 232, 178, 185, 112, 104,
				218, 246, 97, 228, 251, 34, 242, 193, 238, 210, 144, 12, 191, 179, 162, 241,
				81, 51, 145, 235, 249, 14, 239, 107, 49, 192, 214, 31, 181, 199, 106, 157,
				184, 84, 204, 176, 115, 121, 50, 45, 127, 4, 150, 254, 138, 236, 205, 93,
				222, 114, 67, 29, 24, 72, 243, 141, 128, 195, 78, 66, 215
			};
			return kPermutation;
		}
	};
}
