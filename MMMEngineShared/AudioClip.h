#pragma once

#include "Export.h"
#include "Resource.h"
#include "rttr/type"
#include "rttr/registration_friend.h"
#include <cstdint>
#include <vector>

namespace FMOD
{
	class Sound;
}

namespace MMMEngine
{
	class MMMENGINE_API AudioClip : public Resource
	{
	private:
		RTTR_ENABLE(Resource)
		RTTR_REGISTRATION_FRIEND
		friend class ResourceManager;
		friend class SceneManager;
		friend class Scene;

		FMOD::Sound* m_sound = nullptr;
		std::vector<uint8_t> m_embeddedData;

	public:
		AudioClip() = default;
		~AudioClip() override;

		bool LoadFromFilePath(const std::wstring& filePath) override;

		FMOD::Sound* GetSound() const { return m_sound; }
	};
}
