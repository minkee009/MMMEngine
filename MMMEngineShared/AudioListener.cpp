#include "AudioListener.h"

#include "AudioManager.h"
#include "Transform.h"

#include <fmod.hpp>
#include <rttr/registration>

namespace
{
	FMOD_VECTOR ToFmodVector(const DirectX::SimpleMath::Vector3& v)
	{
		return { v.x, v.y, v.z };
	}
}

RTTR_REGISTRATION
{
	using namespace rttr;
	using namespace MMMEngine;

	registration::class_<AudioListener>("AudioListener")
		(rttr::metadata("wrapper_type_name", "ObjPtr<AudioListener>"))
		.property_readonly("IsMainListener", &AudioListener::IsMainListener);

	registration::class_<ObjPtr<AudioListener>>("ObjPtr<AudioListener>")
		.constructor<>([]() { return Object::NewObject<AudioListener>(); })
		.method("Inject", &ObjPtr<AudioListener>::Inject);
}

void MMMEngine::AudioListener::Initialize()
{
	Behaviour::Initialize();
	m_hasLastPosition = false;
	AudioManager::Get().RegisterListener(this);
}

void MMMEngine::AudioListener::UnInitialize()
{
	AudioManager::Get().UnregisterListener(this);
	Behaviour::UnInitialize();
}

void MMMEngine::AudioListener::SetAsMainListener()
{
	AudioManager::Get().SetMainListener(this);
}

void MMMEngine::AudioListener::SetMainFlag(bool value)
{
	m_isMain = value;
}

void MMMEngine::AudioListener::BuildAttributes(float dt, FMOD_VECTOR& outPos, FMOD_VECTOR& outVel, FMOD_VECTOR& outForward, FMOD_VECTOR& outUp)
{
	auto tr = GetTransform();
	if (!tr.IsValid())
		return;

	using namespace DirectX::SimpleMath;
	Vector3 position = tr->GetWorldPosition();
	Vector3 forward = tr->GetWorldMatrix().Backward();
	Vector3 up = tr->GetWorldMatrix().Up();

	if (forward.LengthSquared() > 1e-6f)
		forward.Normalize();
	else
		forward = Vector3::UnitZ;

	if (up.LengthSquared() > 1e-6f)
		up.Normalize();
	else
		up = Vector3::UnitY;

	Vector3 velocity = Vector3::Zero;
	if (m_hasLastPosition && dt > 0.0f)
		velocity = (position - m_lastPosition) / dt;

	m_lastPosition = position;
	m_hasLastPosition = true;

	outPos = ToFmodVector(position);
	outVel = ToFmodVector(velocity);
	outForward = ToFmodVector(forward);
	outUp = ToFmodVector(up);
}
