#pragma once
#include "ExportSingleton.hpp"
#include "Scene.h"

namespace MMMEngine
{
	class MMMENGINE_API SceneSerializer : public Utility::ExportSingleton<SceneSerializer>
	{
	public:
		void Serialize(const Scene& scene, std::wstring path);
		void Deserialize(Scene& scene, const SnapShot& snapshot);

		SnapShot SerializeToMemory(const Scene& scene);

		void ExtractScenes(const std::vector<Scene*>& scenes, const std::wstring& rootPath);
	};
}