#pragma once
#include "ExportSingleton.hpp"
#include "Scene.h"

namespace MMMEngine
{
	/// 씬/컴포넌트 직렬화·역직렬화.
	/// SerializableEvent, SerializableEventT<float> 프로퍼티는 자동으로
	/// { TargetMUID, MessageName } 배열 형태로 저장·복원되며, Deserialize 후
	/// ObjectManager의 MUID 테이블과 리졸버가 설정되어 Invoke() 시 대상 Behaviour로 CallMessage가 호출됨.
	class MMMENGINE_API SceneSerializer : public Utility::ExportSingleton<SceneSerializer>
	{
	public:
		void Serialize(const Scene& scene, std::wstring path);
		void Deserialize(Scene& scene, const SnapShot& snapshot);

		void SerializeToMemory(const Scene& scene, SnapShot& snapshot, bool makeDefaultObjects = false);

		void ExtractScenesList(const std::vector<Scene*>& scenes, const std::wstring& rootPath);
	};
}