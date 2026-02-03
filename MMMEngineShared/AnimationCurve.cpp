#include "pch.h"
#include "AnimationCurve.h"
#include "rttr/registration"

#include <algorithm>

RTTR_REGISTRATION
{
	using namespace rttr;
	using namespace MMMEngine;
	registration::class_<CurveKeyframe>("CurveKeyframe")
		.constructor<>()(rttr::policy::ctor::as_object)
		.property("time", &CurveKeyframe::time)
		.property("value", &CurveKeyframe::value)
		.property("inTangent", &CurveKeyframe::inTangent)
		.property("outTangent", &CurveKeyframe::outTangent)
		.property("tangentMode", &CurveKeyframe::tangentMode);
	registration::class_<AnimationCurve>("AnimationCurve")
		.constructor<>()(rttr::policy::ctor::as_object)
		.property("keyframes", &AnimationCurve::GetKeyframes, &AnimationCurve::SetKeyframes);
}

namespace MMMEngine
{
	CurveKeyframe::CurveKeyframe() = default;

	CurveKeyframe::CurveKeyframe(float t, float v, float inTan, float outTan, int mode)
		: time(t)
		, value(v)
		, inTangent(inTan)
		, outTangent(outTan)
		, tangentMode(mode)
	{
	}

	AnimationCurve::AnimationCurve() = default;

	void AnimationCurve::Clear()
	{
		mKeyframes.clear();
	}

	bool AnimationCurve::IsEmpty() const
	{
		return mKeyframes.empty();
	}

	void AnimationCurve::AddKeyframe(float time, float value, float inTan, float outTan, int mode)
	{
		mKeyframes.emplace_back(time, value, inTan, outTan, mode);
		SortKeyframes();
	}

	const std::vector<CurveKeyframe>& AnimationCurve::GetKeyframes() const
	{
		return mKeyframes;
	}

	void AnimationCurve::SetKeyframes(const std::vector<CurveKeyframe>& keyframes)
	{
		mKeyframes = keyframes;
		SortKeyframes();
	}

	float AnimationCurve::Evaluate(float time) const
	{
		if (mKeyframes.empty())
		{
			return 0.0f;
		}

		if (mKeyframes.size() == 1)
		{
			return mKeyframes[0].value;
		}

		if (time <= mKeyframes.front().time)
		{
			return mKeyframes.front().value;
		}
		if (time >= mKeyframes.back().time)
		{
			return mKeyframes.back().value;
		}

		for (size_t i = 0; i + 1 < mKeyframes.size(); ++i)
		{
			const CurveKeyframe& kf0 = mKeyframes[i];
			const CurveKeyframe& kf1 = mKeyframes[i + 1];

			if (kf0.time <= time && time <= kf1.time)
			{
				const float dt = kf1.time - kf0.time;
				if (dt <= 0.0f)
				{
					return kf0.value;
				}

				const float t = (time - kf0.time) / dt;
				const float t2 = t * t;
				const float t3 = t2 * t;

				const float h00 = 2.0f * t3 - 3.0f * t2 + 1.0f;
				const float h10 = t3 - 2.0f * t2 + t;
				const float h01 = -2.0f * t3 + 3.0f * t2;
				const float h11 = t3 - t2;

				const float m0 = kf0.outTangent * dt;
				const float m1 = kf1.inTangent * dt;

				return h00 * kf0.value + h10 * m0 + h01 * kf1.value + h11 * m1;
			}
		}

		return 0.0f;
	}

	void AnimationCurve::SortKeyframes()
	{
		std::sort(mKeyframes.begin(), mKeyframes.end(),
			[](const CurveKeyframe& a, const CurveKeyframe& b)
			{
				return a.time < b.time;
			});
	}
}
