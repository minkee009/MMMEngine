#pragma once
#include "ScriptBehaviour.h"
#include "MMMApplication.h"
#include "Export.h"
#include "rttr/type"

namespace MMMEngine {
	class MMMENGINE_API GameManager : public ScriptBehaviour
	{
		RTTR_ENABLE(ScriptBehaviour)
		RTTR_REGISTRATION_FRIEND
	public:
		GameManager()
		{
			REGISTER_BEHAVIOUR_MESSAGE(Start)
			REGISTER_BEHAVIOUR_MESSAGE(Update)
		}
		void Initialize() override;
		void UnInitialize() override;
		void Start();
		void Update();
		static ObjPtr<GameManager> instance;
	private:
		bool GameOver = false;
		bool nowSetting = true;
		float settingTimer = 0.0f;
		float settingfullTime = 30.0f;
		float NormalSpawnTimer = 0.0f;
		float NormalSpawnDelay = 10.0f;
		ObjPtr<GameObject> player;
		ObjPtr<GameObject> castle;
	};
}
