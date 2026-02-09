#include "AudioManager.h"

#include "AudioListener.h"
#include "AudioSource.h"

#include <fmod.hpp>
#include <fmod_errors.h>
#include <iostream>

DEFINE_SINGLETON(MMMEngine::AudioManager)

namespace
{
	bool CheckFmod(FMOD_RESULT result, const char* context)
	{
		if (result == FMOD_OK)
			return true;

		std::cout << "[FMOD] " << context << " failed: " << FMOD_ErrorString(result) << std::endl;
		return false;
	}
}

bool MMMEngine::AudioManager::StartUp()
{
	if (m_initialized)
		return true;

	FMOD_RESULT result = FMOD::System_Create(&m_system);
	if (!CheckFmod(result, "System_Create"))
	{
		m_system = nullptr;
		return false;
	}

	result = m_system->init(512, FMOD_INIT_NORMAL, nullptr);
	if (!CheckFmod(result, "System::init"))
	{
		m_system->release();
		m_system = nullptr;
		return false;
	}

	CheckFmod(m_system->set3DSettings(1.0f, 1.0f, 1.0f), "System::set3DSettings");
	CheckFmod(m_system->getMasterChannelGroup(&m_master), "System::getMasterChannelGroup");
	if (m_master)
		CheckFmod(m_master->setPaused(m_masterPaused), "ChannelGroup::setPaused");

	m_initialized = true;
	return true;
}

void MMMEngine::AudioManager::ShutDown()
{
	StopAll();

	m_sources.clear();
	m_listeners.clear();
	m_mainListener = nullptr;

	if (m_system)
	{
		m_system->close();
		m_system->release();
		m_system = nullptr;
	}

	m_master = nullptr;
	m_initialized = false;
	m_masterPaused = false;
}

void MMMEngine::AudioManager::Update(float dt)
{
	if (!m_initialized || !m_system)
		return;

	AudioListener* activeListener = nullptr;
	if (m_mainListener && m_mainListener->IsActiveAndEnabled())
	{
		activeListener = m_mainListener;
	}
	else
	{
		for (auto* listener : m_listeners)
		{
			if (listener && listener->IsActiveAndEnabled())
			{
				activeListener = listener;
				break;
			}
		}
	}

	if (activeListener)
	{
		FMOD_VECTOR pos{};
		FMOD_VECTOR vel{};
		FMOD_VECTOR forward{ 0.0f, 0.0f, 1.0f };
		FMOD_VECTOR up{ 0.0f, 1.0f, 0.0f };
		activeListener->BuildAttributes(dt, pos, vel, forward, up);
		CheckFmod(m_system->set3DListenerAttributes(0, &pos, &vel, &forward, &up), "System::set3DListenerAttributes");
	}
	else
	{
		FMOD_VECTOR pos{ 0.0f, 0.0f, 0.0f };
		FMOD_VECTOR vel{ 0.0f, 0.0f, 0.0f };
		FMOD_VECTOR forward{ 0.0f, 0.0f, 1.0f };
		FMOD_VECTOR up{ 0.0f, 1.0f, 0.0f };
		CheckFmod(m_system->set3DListenerAttributes(0, &pos, &vel, &forward, &up), "System::set3DListenerAttributes");
	}

	for (auto* source : m_sources)
	{
		if (source)
			source->UpdateAudio(dt);
	}

	CheckFmod(m_system->update(), "System::update");
}

void MMMEngine::AudioManager::StopAll()
{
	for (auto* source : m_sources)
	{
		if (source)
			source->Stop();
	}

	if (m_master)
		m_master->stop();
}

void MMMEngine::AudioManager::SetPaused(bool paused)
{
	if (m_masterPaused == paused)
		return;

	m_masterPaused = paused;
	if (!m_initialized || !m_master)
		return;

	CheckFmod(m_master->setPaused(paused), "ChannelGroup::setPaused");
}

void MMMEngine::AudioManager::RegisterSource(AudioSource* source)
{
	if (!source)
		return;
	m_sources.insert(source);
}

void MMMEngine::AudioManager::UnregisterSource(AudioSource* source)
{
	if (!source)
		return;
	m_sources.erase(source);
}

void MMMEngine::AudioManager::RegisterListener(AudioListener* listener)
{
	if (!listener)
		return;
	m_listeners.insert(listener);
	if (!m_mainListener)
		SetMainListener(listener);
}

void MMMEngine::AudioManager::UnregisterListener(AudioListener* listener)
{
	if (!listener)
		return;

	m_listeners.erase(listener);
	if (m_mainListener == listener)
		m_mainListener = nullptr;
}

void MMMEngine::AudioManager::SetMainListener(AudioListener* listener)
{
	if (m_mainListener == listener)
		return;

	if (m_mainListener)
		m_mainListener->SetMainFlag(false);

	m_mainListener = listener;
	if (m_mainListener)
		m_mainListener->SetMainFlag(true);
}
