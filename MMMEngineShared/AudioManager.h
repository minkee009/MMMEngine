#pragma once

#include "Export.h"
#include "ExportSingleton.hpp"

#include <unordered_set>

namespace FMOD
{
	class System;
	class ChannelGroup;
}

namespace MMMEngine
{
	class AudioSource;
	class AudioListener;

	class MMMENGINE_API AudioManager : public Utility::ExportSingleton<AudioManager>
	{
		friend class Utility::ExportSingleton<AudioManager>;
	private:
		AudioManager() = default;

		FMOD::System* m_system = nullptr;
		FMOD::ChannelGroup* m_master = nullptr;

		std::unordered_set<AudioSource*> m_sources;
		std::unordered_set<AudioListener*> m_listeners;
		AudioListener* m_mainListener = nullptr;

		bool m_initialized = false;

	public:
		bool StartUp();
		void ShutDown();
		void Update(float dt);
		void StopAll();

		bool IsInitialized() const { return m_initialized; }
		FMOD::System* GetSystem() const { return m_system; }
		FMOD::ChannelGroup* GetMasterGroup() const { return m_master; }

		void RegisterSource(AudioSource* source);
		void UnregisterSource(AudioSource* source);

		void RegisterListener(AudioListener* listener);
		void UnregisterListener(AudioListener* listener);

		void SetMainListener(AudioListener* listener);
		AudioListener* GetMainListener() const { return m_mainListener; }
	};
}
