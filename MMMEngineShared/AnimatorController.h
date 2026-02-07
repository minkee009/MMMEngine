#pragma once
#include "Export.h"
#include "Component.h"
#include "ResourceManager.h"

#include "rttr/type"

namespace MMMEngine {
	class Animator;
	class SkinRenderer;

	enum class CondOp { Greater, Less, Equal, NotEqual };
	enum class ParamType { Bool, Float, Trigger };

	struct AnimParameter
	{
		ParamType type;
		float value;
	};
	struct AnimCondition
	{
		std::string param;
		CondOp op;
		float value; // bool/trigger는 0/1로 처리
	};
	struct AnimTransition {
		std::string toState;
		float blendTime = 0.2f;

		bool hasExitTime = false;
		float exitTime = 0.9f; // normalized time

		std::vector<AnimCondition> conditions;
	};
	struct AnimState {
		std::string clipName;
		bool loop = true;
		float speed = 1.0f;
		int rootIdx = -1;

		std::vector<AnimTransition> transitions;
	};
	
	class MMMENGINE_API AnimatorController : public Component
	{
		RTTR_ENABLE(Component)
		RTTR_REGISTRATION_FRIEND
	private:
		ObjPtr<Animator> mAnimator;
		ObjPtr<SkinRenderer> mSkinRenderer;

		std::unordered_map<std::string, AnimState> mStates;
		std::unordered_map<std::string, AnimParameter> mParams;

		AnimState* mCurrent = nullptr;
		AnimState* mNext = nullptr;           // 전이 중일 때
		AnimTransition* mActiveTr = nullptr;  // 현재 전이
		float mTransitionTime = 0.0f;

		float GetCurrentNormalizedTime();
		bool EvalCondition(const AnimCondition& cond);
		void ResetTriggers();
		bool CheckTransition(const AnimTransition& tr);
		void BeginTransition(AnimTransition& tr);
		void PlayCurrentState();
		void BlendStates(float _weight);
	public:
		void Initialize() override;
		void UnInitialize() override;

		void Update(float dt);
		void AddTrigger(std::string name);
		void SetTrigger(std::string name);
		void SetBool(std::string name, bool v);
		void SetFloat(std::string name, float v);
	
		void AddState(std::string _stateName, const AnimState& _animState);
		void SetState(std::string _stateName, const AnimState& _animState);
		bool HasState(const std::string& stateName) const;
		const AnimState* GetState(const std::string& stateName) const;
		AnimState* GetStateMutable(const std::string& stateName);

		int AddTransition(std::string _stateName, const AnimTransition& _transition);
		void RemoveTransition(const std::string& stateName, int transitionIndex);
		void SetTransitionBlendTime(const std::string& stateName, int transitionIndex, float seconds);
		void SetTransitionExitTime(const std::string& stateName, int transitionIndex, bool enabled, float normalizedExitTime);
	
		void SetEntryState(const std::string& stateName);
	};
}


