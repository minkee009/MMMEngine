#pragma once
#include "Resource.h"
#include <fmod.hpp>
#include <string>

namespace MMMEngine
{
	class SoundResource : public Resource
	{
		RTTR_ENABLE(Resource);
		RTTR_REGISTRATION_FRIEND
		friend class ResourceManager;
		friend class AudioManager;
	public:
		enum class Kind { BGM, SFX2D, SFX3D };

		SoundResource() = default;
		~SoundResource() override;

		FMOD::Sound* GetSound() const { return m_sound; }
		Kind GetKind() const { return m_kind; }

		// 메타 접근(필요하면)
		const std::wstring& GetAudioPath() const { return m_audioPath; }
		bool IsLoop() const { return m_loop; }
		float GetMinDist() const { return m_minDist; }
		float GetMaxDist() const { return m_maxDist; }

	protected:
		// filePath == ".sound" 메타 파일 경로
		bool LoadFromFilePath(const std::wstring& filePath) override;

	private:
		// meta (.sound)로부터 채워짐
		std::wstring m_audioPath;
		Kind  m_kind = Kind::SFX2D;
		bool  m_loop = false;
		float m_minDist = 1.f;
		float m_maxDist = 20.f;

		FMOD::Sound* m_sound = nullptr;
	};
}

