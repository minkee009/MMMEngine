#include "pch.h"
#include "AnimatorController.h"
#include "AnimationClip.h"
#include "Animator.h"
#include "rttr/registration"

RTTR_REGISTRATION
{
	using namespace rttr;
	using namespace MMMEngine;

	registration::class_<AnimatorController>("AnimatorController")
		(rttr::metadata("wrapper_type_name", "ObjPtr<AnimatorController>"));

registration::class_<ObjPtr<AnimatorController>>("ObjPtr<AnimatorController>")
	.constructor<>(
		[]() {
			return Object::NewObject<AnimatorController>();
		})
	.method("Inject", &ObjPtr<AnimatorController>::Inject);
}

namespace
{
	inline float Clamp01(float v)
	{
		if (v < 0.0f) return 0.0f;
		if (v > 1.0f) return 1.0f;
		return v;
	}

	inline float ClampMin(float v, float minV)
	{
		return (v < minV) ? minV : v;
	}
}

float MMMEngine::AnimatorController::GetCurrentNormalizedTime()
{
	if (!mCurrent)
		return 0.0f;

	auto it = mAnimator->mCurrentPlayingMap.find(mCurrent->clipName);
	if (it == mAnimator->mCurrentPlayingMap.end())
		return 0.0f;

	const AnimInfo& info = it->second;
	auto clip = mAnimator->GetAnimClip(info.clipIdx);
	if (!clip || clip->durationSec <= 0.0f)
		return 0.0f;

	float t = info.elipsedTime / clip->durationSec;

	if (info.isLoop)
		t = std::fmod(t, 1.0f);

	return t;
}

bool MMMEngine::AnimatorController::EvalCondition(const AnimCondition& cond)
{
	auto it = mParams.find(cond.param);
	if (it == mParams.end())
		return false;

	const AnimParameter& p = it->second;
	float v = p.value;

	switch (cond.op)
	{
	case CondOp::Greater:   return v > cond.value;
	case CondOp::Less:      return v < cond.value;
	case CondOp::Equal:     return fabs(v - cond.value) < 0.0001f;
	case CondOp::NotEqual:  return fabs(v - cond.value) >= 0.0001f;
	}
	return false;
}

void MMMEngine::AnimatorController::Initialize()
{
	mAnimator = GetComponent<Animator>();
	if(!mAnimator)
		Destroy(SelfPtr(this));
}

void MMMEngine::AnimatorController::UnInitialize()
{
	// 전이/상태 관련 런타임 포인터 정리
	mTransitionTime = 0.0f;
	mActiveTr = nullptr;
	mNext = nullptr;
	mCurrent = nullptr;

	ResetTriggers();

	if (mAnimator.IsValid() && !mAnimator->IsDestroyed())
	{
		mAnimator->StopClip();
	}

	mAnimator.Reset();
	mStates.clear();
	mParams.clear();
}

void MMMEngine::AnimatorController::Update(float dt)
{
	if (!mAnimator || !mCurrent) return;

    if (mActiveTr)
    {
        mTransitionTime += dt;

        float denom = (mActiveTr->blendTime <= 0.00001f) ? 0.00001f : mActiveTr->blendTime;
        float w = Clamp01(mTransitionTime / denom);

        if (w >= 1.0f)
        {
            mCurrent = mNext;
            mNext = nullptr;
            mActiveTr = nullptr;
            mTransitionTime = 0.0f;

            ResetTriggers();
            PlayCurrentState();
        }
        else
        {
            BlendStates(w);
        }
        return;
    }

    for (auto& tr : mCurrent->transitions)
    {
        if (CheckTransition(tr))
        {
            BeginTransition(tr);
            return;
        }
    }
}

void MMMEngine::AnimatorController::SetTrigger(std::string name)
{
	auto it = mParams.find(name);
	if(it != mParams.end() && it->second.type == ParamType::Trigger)
		it->second.value = 1.0f;
}

void MMMEngine::AnimatorController::ResetTriggers()
{
	for (auto& [k, p] : mParams)
		if (p.type == ParamType::Trigger)
			p.value = 0.0f;
}

void MMMEngine::AnimatorController::SetBool(std::string name, bool v)
{
	mParams[name].type = ParamType::Bool;
	mParams[name].value = v;
}

void MMMEngine::AnimatorController::SetFloat(std::string name, float v)
{
	mParams[name].type = ParamType::Float;
	mParams[name].value = v;
}

void MMMEngine::AnimatorController::AddState(std::string _stateName, const AnimState& _animState)
{
	mStates[_stateName] = _animState;
}

void MMMEngine::AnimatorController::SetState(std::string _stateName, const AnimState& _animState)
{
	auto it = mStates.find(_stateName);
	if (it != mStates.end())
		it->second = _animState;
}

bool MMMEngine::AnimatorController::HasState(const std::string& stateName) const
{
	auto it = mStates.find(stateName);
	if (it != mStates.end())
		return true;
	return false;
}

const MMMEngine::AnimState* MMMEngine::AnimatorController::GetState(const std::string& stateName) const
{
	auto it = mStates.find(stateName);
	if (it != mStates.end())
		return &it->second;
	return nullptr;
}

MMMEngine::AnimState* MMMEngine::AnimatorController::GetStateMutable(const std::string& stateName)
{
	auto it = mStates.find(stateName);
	if (it != mStates.end())
		return &it->second;
	return nullptr;
}

int MMMEngine::AnimatorController::AddTransition(std::string _stateName, const AnimTransition& _transition)
{
	auto it = mStates.find(_stateName);
	if (it == mStates.end())
		return -1;
	
	auto& trans = it->second.transitions;
	auto tIt = std::find_if(trans.begin(), trans.end(), 
		[&](const AnimTransition& t) {
			return t.toState == _transition.toState;
		});

	if (tIt != trans.end()) {
		*tIt = _transition;
		return static_cast<int>(tIt - trans.begin());
	}
	
	int idx = static_cast<int>(trans.size());
	trans.push_back(_transition);
	return idx;
}

void MMMEngine::AnimatorController::RemoveTransition(const std::string& stateName, int transitionIndex)
{
	auto it = mStates.find(stateName);
	if (it == mStates.end())
		return;

	auto& trans = it->second.transitions;
	if (transitionIndex < 0 || transitionIndex >= static_cast<int>(trans.size()))
		return;

	trans.erase(trans.begin() + transitionIndex);
}

void MMMEngine::AnimatorController::SetTransitionBlendTime(const std::string& stateName, int transitionIndex, float seconds)
{
	auto it = mStates.find(stateName);
	if (it == mStates.end())
		return;

	auto& trans = it->second.transitions;
	if (transitionIndex < 0 || transitionIndex >= static_cast<int>(trans.size()))
		return;

	trans[transitionIndex].blendTime = ClampMin(seconds, 0.0f);
}

void MMMEngine::AnimatorController::SetTransitionExitTime(const std::string& stateName, int transitionIndex, bool enabled, float normalizedExitTime)
{
	auto it = mStates.find(stateName);
	if (it == mStates.end())
		return;

	auto& trans = it->second.transitions;
	if (transitionIndex < 0 || transitionIndex >= static_cast<int>(trans.size()))
		return;

	auto& tr = trans[transitionIndex];
	tr.hasExitTime = enabled;

	// exitTime은 normalized 0~1로 관리 (루프 상태면 매 루프 기준)
	tr.exitTime = Clamp01(normalizedExitTime);
}

void MMMEngine::AnimatorController::SetEntryState(const std::string& stateName)
{
	auto it = mStates.find(stateName);
	if (it == mStates.end()) return;
	mCurrent = &it->second;
	mNext = nullptr;
	mActiveTr = nullptr;
	mTransitionTime = 0.0f;
	PlayCurrentState();
}

bool MMMEngine::AnimatorController::CheckTransition(const AnimTransition& tr)
{
	// exit time 체크
	if (tr.hasExitTime)
	{
		float nt = GetCurrentNormalizedTime();
		if (nt < tr.exitTime)
			return false;
	}

	// parameter 조건
	for (auto& c : tr.conditions)
	{
		if (!EvalCondition(c))
			return false;
	}
	return true;
}

void MMMEngine::AnimatorController::BeginTransition(AnimTransition& tr)
{
	if (!mAnimator || !mCurrent) return;

	auto itNext = mStates.find(tr.toState);
	if (itNext == mStates.end())
		return; // or log

	mActiveTr = &tr;
	mTransitionTime = 0.0f;
	mNext = &itNext->second;

	// Animator에 두 클립 동시에 등록
	mAnimator->PlayClip(mCurrent->clipName, mCurrent->loop, mCurrent->rootIdx);
	mAnimator->PlayBlendClip(mNext->clipName, 0.0f, mNext->loop, mNext->rootIdx);
}

void MMMEngine::AnimatorController::PlayCurrentState()
{
	if (!mAnimator || !mCurrent) return;
	mAnimator->PlayClip(mCurrent->clipName, mCurrent->loop, mCurrent->rootIdx);
}

void MMMEngine::AnimatorController::BlendStates(float _weight)
{
	mAnimator->PlayBlendClip(mCurrent->clipName, 1.0f - _weight, mCurrent->loop, mCurrent->rootIdx);
	mAnimator->PlayBlendClip(mNext->clipName, _weight, mNext->loop, mNext->rootIdx);
}
