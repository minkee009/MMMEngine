#include "pch.h"
#include "Animator.h"
#include <SkinRenderer.h>

MMMEngine::Mesh_VecKey MMMEngine::Animator::Evaluate(const Mesh_VecKey& _k1, const Mesh_VecKey& _k2, float _currTime)
{
	if (_k1.value == _k2.value)
		return _k1;

	float lerpTime = (_currTime - _k1.timeSec) / (_k2.timeSec - _k1.timeSec);

	return {
		_currTime,
		_k1.value + (_k2.value - _k1.value) * lerpTime
	};
}

MMMEngine::Mesh_QuatKey MMMEngine::Animator::Evaluate(const Mesh_QuatKey& _k1, const Mesh_QuatKey& _k2, float _currTime)
{
	if (_k1.value == _k2.value)
		return _k1;

	float lerpTime = (_currTime - _k1.timeSec) / (_k2.timeSec - _k1.timeSec);

	DirectX::SimpleMath::Quaternion temp;
	temp = DirectX::SimpleMath::Quaternion::Slerp(_k1.value, _k2.value, lerpTime);
	return {
		_currTime,
		temp };
}

void MMMEngine::Animator::Initialize()
{
	auto skinRenderer = GetComponent<SkinRenderer>();

	if (!skinRenderer)
		Set;
		
}

void MMMEngine::Animator::UnInitialize()
{

}
