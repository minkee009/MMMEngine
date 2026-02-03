#pragma once
#include <vector>
#include "rttr/type"
#include "rttr/registration_friend.h"

#include "Export.h"

namespace MMMEngine
{
	struct MMMENGINE_API CurveKeyframe
	{
		float time = 0.0f;
		float value = 0.0f;
		float inTangent = 0.0f;
		float outTangent = 0.0f;
		int tangentMode = 1; // 0=broken, 1=unified

		CurveKeyframe();
		CurveKeyframe(float t, float v, float inTan = 0.0f, float outTan = 0.0f, int mode = 1);
	};

	class MMMENGINE_API AnimationCurve
	{
		RTTR_ENABLE()
		RTTR_REGISTRATION_FRIEND
	public:
		AnimationCurve();

		void Clear();
		bool IsEmpty() const;

		void AddKeyframe(float time, float value, float inTan = 0.0f, float outTan = 0.0f, int mode = 1);
		const std::vector<CurveKeyframe>& GetKeyframes() const;
		void SetKeyframes(const std::vector<CurveKeyframe>& keyframes);

		float Evaluate(float time) const;

	private:
		std::vector<CurveKeyframe> mKeyframes;
		void SortKeyframes();
	};
}
