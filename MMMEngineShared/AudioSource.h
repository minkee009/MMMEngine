#pragma once

#include "Behaviour.h"
#include "Export.h"
#include "ResourceManager.h"
#include "SimpleMath.h"

namespace FMOD
{
	class Channel;
}

namespace MMMEngine
{
	class AudioClip;

	class MMMENGINE_API AudioSource : public Behaviour
	{
	private:
		RTTR_ENABLE(Behaviour)
		RTTR_REGISTRATION_FRIEND

		ResPtr<AudioClip> m_clip = nullptr;
		FMOD::Channel* m_channel = nullptr;

		bool m_playOnAwake = false;
		bool m_loop = false;
		bool m_spatialize = true;

		float m_volume = 1.0f;
		float m_pitch = 1.0f;
		float m_pan = 0.0f;
		float m_minDistance = 1.0f;
		float m_maxDistance = 100.0f;

		DirectX::SimpleMath::Vector3 m_lastPosition = DirectX::SimpleMath::Vector3::Zero;
		bool m_hasLastPosition = false;
		bool m_wasActive = false;

		void ApplyChannelSettings();
		void Update3DAttributes(float dt);

	public:
		AudioSource() = default;

		void Initialize() override;
		void UnInitialize() override;

		void UpdateAudio(float dt);

		void Play();
		void Stop();
		void Pause();
		void Resume();

		const ResPtr<AudioClip>& GetClip() const { return m_clip; }
		void SetClip(const ResPtr<AudioClip>& clip);

		bool GetPlayOnAwake() const { return m_playOnAwake; }
		void SetPlayOnAwake(bool value) { m_playOnAwake = value; }

		bool GetLoop() const { return m_loop; }
		void SetLoop(bool value);

		bool GetSpatialize() const { return m_spatialize; }
		void SetSpatialize(bool value);

		float GetVolume() const { return m_volume; }
		void SetVolume(float value);

		float GetPitch() const { return m_pitch; }
		void SetPitch(float value);

		float GetPan() const { return m_pan; }
		void SetPan(float value);

		float GetMinDistance() const { return m_minDistance; }
		void SetMinDistance(float value);

		float GetMaxDistance() const { return m_maxDistance; }
		void SetMaxDistance(float value);

		bool IsPlaying() const;
	};
}
