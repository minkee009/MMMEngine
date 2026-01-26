#pragma once
#include "ScriptBehaviour.h"
#include "MMMApplication.h"

namespace MMMEngine {
	class GameManager : public ScriptBehaviour
	{
	public:
		void Initialize() override;
		void UnInitialize() override;
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
