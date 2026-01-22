#pragma once
#include "ScriptBehaviour.h"
#include "MMMApplication.h"
#include <vector>

namespace MMMEngine {
	class SnowballManager : public ScriptBehaviour
	{
	public:
		void Initialize() override;
		void UnInitialize() override;
		void Update();
	private:
		void MakeSnowball();
		ObjPtr<GameObject> player;
		ObjPtr<GameObject> matchedSnowball = nullptr;
		ObjPtr<GameObject> castle;

		float spawnTimer = 0.0f;
		float spawnDelay = 0.2f; //눈 생성 타이밍
		float pickupRange = 1.0f; //눈 픽업 거리
		float CoinupRange = 1.0f; //성에 눈이 들어가는 거리
		float offset = 1.5f;  //눈 생성 시 플레이어와의 거리
		std::vector<ObjPtr<GameObject>> Snows;
	};
}

