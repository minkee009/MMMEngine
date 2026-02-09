#include "AudioSource.h"

#include "AudioManager.h"
#include "AudioClip.h"
#include "Transform.h"

#include <fmod.hpp>
#include <fmod_errors.h>
#include <iostream>
#include <rttr/registration>

namespace
{
	bool CheckFmod(FMOD_RESULT result, const char* context)
	{
		if (result == FMOD_OK)
			return true;

		std::cout << "[FMOD] " << context << " failed: " << FMOD_ErrorString(result) << std::endl;
		return false;
	}

	FMOD_VECTOR ToFmodVector(const DirectX::SimpleMath::Vector3& v)
	{
		return { v.x, v.y, v.z };
	}
}

RTTR_REGISTRATION
{
	using namespace rttr;
	using namespace MMMEngine;

	registration::class_<AudioSource>("AudioSource")
		(rttr::metadata("wrapper_type_name", "ObjPtr<AudioSource>"))
		.property("Clip", &AudioSource::GetClip, &AudioSource::SetClip)
		.property("PlayOnAwake", &AudioSource::GetPlayOnAwake, &AudioSource::SetPlayOnAwake)
		.property("Loop", &AudioSource::GetLoop, &AudioSource::SetLoop)
		.property("Spatialize", &AudioSource::GetSpatialize, &AudioSource::SetSpatialize)
		.property("Volume", &AudioSource::GetVolume, &AudioSource::SetVolume)
		.property("Pitch", &AudioSource::GetPitch, &AudioSource::SetPitch)
		.property("Pan", &AudioSource::GetPan, &AudioSource::SetPan)
		.property("MinDistance", &AudioSource::GetMinDistance, &AudioSource::SetMinDistance)
		.property("MaxDistance", &AudioSource::GetMaxDistance, &AudioSource::SetMaxDistance);

	registration::class_<ObjPtr<AudioSource>>("ObjPtr<AudioSource>")
		.constructor<>([]() { return Object::NewObject<AudioSource>(); })
		.method("Inject", &ObjPtr<AudioSource>::Inject);
}

void MMMEngine::AudioSource::Initialize()
{
	Behaviour::Initialize();

	AudioManager::Get().RegisterSource(this);

	REGISTER_BEHAVIOUR_MESSAGE(Play);
	REGISTER_BEHAVIOUR_MESSAGE(Stop);
	REGISTER_BEHAVIOUR_MESSAGE(Pause);
	REGISTER_BEHAVIOUR_MESSAGE(Resume);
}

void MMMEngine::AudioSource::UnInitialize()
{
	Stop();
	AudioManager::Get().UnregisterSource(this);
	Behaviour::UnInitialize();
}

void MMMEngine::AudioSource::UpdateAudio(float dt)
{
	const bool active = IsActiveAndEnabled();
	if (!active)
	{
		if (m_channel)
			Stop();
		m_wasActive = false;
		m_hasLastPosition = false;
		return;
	}

	if (!m_wasActive)
	{
		if (m_playOnAwake)
			Play();
		m_wasActive = true;
	}

	if (!m_channel)
		return;

	bool playing = false;
	if (!CheckFmod(m_channel->isPlaying(&playing), "Channel::isPlaying") || !playing)
	{
		m_channel = nullptr;
		return;
	}

	ApplyChannelSettings();
	Update3DAttributes(dt);
}

void MMMEngine::AudioSource::Play()
{
	if (!AudioManager::Get().IsInitialized())
		return;

	if (!m_clip)
		return;

	auto* sound = m_clip->GetSound();
	if (!sound)
		return;

	auto* system = AudioManager::Get().GetSystem();
	if (!system)
		return;

	auto* group = AudioManager::Get().GetMasterGroup();
	FMOD::Channel* channel = nullptr;
	if (!CheckFmod(system->playSound(sound, group, true, &channel), "System::playSound"))
		return;

	m_channel = channel;
	ApplyChannelSettings();
	Update3DAttributes(0.0f);
	CheckFmod(m_channel->setPaused(false), "Channel::setPaused(false)");
}

void MMMEngine::AudioSource::Stop()
{
	if (m_channel)
	{
		m_channel->stop();
		m_channel = nullptr;
	}
	m_hasLastPosition = false;
}

void MMMEngine::AudioSource::Pause()
{
	if (m_channel)
		CheckFmod(m_channel->setPaused(true), "Channel::setPaused(true)");
}

void MMMEngine::AudioSource::Resume()
{
	if (m_channel)
		CheckFmod(m_channel->setPaused(false), "Channel::setPaused(false)");
}

void MMMEngine::AudioSource::SetClip(const ResPtr<AudioClip>& clip)
{
	if (m_clip == clip)
		return;

	if (m_channel)
		Stop();

	m_clip = clip;
}

void MMMEngine::AudioSource::SetLoop(bool value)
{
	m_loop = value;
	ApplyChannelSettings();
}

void MMMEngine::AudioSource::SetSpatialize(bool value)
{
	m_spatialize = value;
	ApplyChannelSettings();
	if (!m_spatialize)
		m_hasLastPosition = false;
}

void MMMEngine::AudioSource::SetVolume(float value)
{
	m_volume = (value < 0.0f) ? 0.0f : value;
	ApplyChannelSettings();
}

void MMMEngine::AudioSource::SetPitch(float value)
{
	m_pitch = (value < 0.01f) ? 0.01f : value;
	ApplyChannelSettings();
}

void MMMEngine::AudioSource::SetPan(float value)
{
	if (value < -1.0f) value = -1.0f;
	if (value > 1.0f) value = 1.0f;
	m_pan = value;
	ApplyChannelSettings();
}

void MMMEngine::AudioSource::SetMinDistance(float value)
{
	m_minDistance = (value < 0.0f) ? 0.0f : value;
	if (m_maxDistance < m_minDistance)
		m_maxDistance = m_minDistance;
	ApplyChannelSettings();
}

void MMMEngine::AudioSource::SetMaxDistance(float value)
{
	m_maxDistance = (value < m_minDistance) ? m_minDistance : value;
	ApplyChannelSettings();
}

bool MMMEngine::AudioSource::IsPlaying() const
{
	if (!m_channel)
		return false;

	bool playing = false;
	if (m_channel->isPlaying(&playing) != FMOD_OK)
		return false;
	return playing;
}

void MMMEngine::AudioSource::ApplyChannelSettings()
{
	if (!m_channel)
		return;

	FMOD_MODE mode = 0;
	mode |= m_spatialize ? FMOD_3D : FMOD_2D;
	mode |= m_loop ? FMOD_LOOP_NORMAL : FMOD_LOOP_OFF;
	CheckFmod(m_channel->setMode(mode), "Channel::setMode");
	CheckFmod(m_channel->setVolume(m_volume), "Channel::setVolume");
	CheckFmod(m_channel->setPitch(m_pitch), "Channel::setPitch");

	if (!m_spatialize)
	{
		CheckFmod(m_channel->setPan(m_pan), "Channel::setPan");
	}
	else
	{
		CheckFmod(m_channel->set3DMinMaxDistance(m_minDistance, m_maxDistance), "Channel::set3DMinMaxDistance");
	}
}

void MMMEngine::AudioSource::Update3DAttributes(float dt)
{
	if (!m_channel || !m_spatialize)
		return;

	auto tr = GetTransform();
	if (!tr.IsValid())
		return;

	using namespace DirectX::SimpleMath;
	Vector3 position = tr->GetWorldPosition();
	Vector3 velocity = Vector3::Zero;

	if (m_hasLastPosition && dt > 0.0f)
		velocity = (position - m_lastPosition) / dt;

	m_lastPosition = position;
	m_hasLastPosition = true;

	FMOD_VECTOR fpos = ToFmodVector(position);
	FMOD_VECTOR fvel = ToFmodVector(velocity);
	CheckFmod(m_channel->set3DAttributes(&fpos, &fvel), "Channel::set3DAttributes");
}
