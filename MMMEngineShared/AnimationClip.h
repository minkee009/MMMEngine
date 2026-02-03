#pragma once
#include "Export.h"
#include "Resource.h"
#include "RenderShared.h"
#include "rttr/type"

namespace MMMEngine {
	class MMMENGINE_API AnimationClip : public Resource
	{
		RTTR_ENABLE(Resource);
		RTTR_REGISTRATION_FRIEND
			friend class ResourceManager;
			friend class SceneManager;
			friend class Scene;
			friend class Animator;

	public:
		std::string mName;

		float durationSec = 0.0f;
		float ticksPerSecond = 30.0f;
		std::vector<Mesh_AnimTrack> mTracks;

		bool LoadFromFilePath(const std::wstring& filePath) override;
	};
}

