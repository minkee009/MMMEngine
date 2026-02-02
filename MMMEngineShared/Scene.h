#pragma once
#include "Export.h"
#include "Object.h"
#include "MUID.h"
#include "rttr/type"
#include "rttr/registration_friend.h"
#include "json/json.hpp"
#include <vector>

namespace MMMEngine
{
	using SnapShot = nlohmann::json;

	class GameObject;
	class MMMENGINE_API Scene final
	{
	private:
		friend class SceneManager;
		friend class SceneSerializer;
		RTTR_ENABLE()
		RTTR_REGISTRATION_FRIEND
		Utility::MUID m_muid;
		std::string m_name;
		std::vector<ObjPtr<GameObject>> m_gameObjects;
		SnapShot m_snapshot;
		void SetMUID(const Utility::MUID& muid);
		void SetSnapShot(SnapShot&& snapshot) noexcept;  //ȣ��� �ݵ�� ���ڿ� std::move()�� �ű��, ��) loadedScene.SetSnapShot(std::move(snapshot));
		const SnapShot& GetSnapShot() const;
		void Initialize();
		void Clear();
		std::vector<ObjPtr<GameObject>> GetGameObjects();
		ObjPtr<GameObject> CreateGameObject(std::string name);
	public:
		~Scene();
		void RegisterGameObject(ObjPtr<GameObject> go);
		void UnRegisterGameObject(ObjPtr<GameObject> go);
		const std::string& GetName() const;
		const Utility::MUID& GetMUID() const;

		void SetName(const std::string& name);

	};
}
