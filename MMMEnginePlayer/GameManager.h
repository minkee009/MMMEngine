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
	private:
		bool GameOver = false;
		bool nowSetting = true;
		float settingTimer = 30.0f;
		ObjPtr<GameObject> player;
		ObjPtr<GameObject> castle;
	};
}
