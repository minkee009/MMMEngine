#pragma once
#include "ScriptBehaviour.h"
#include "MMMApplication.h"

namespace MMMEngine {
	class EnemySpawner : public ScriptBehaviour
	{
	public:
		void Initialize() override;
		void UnInitialize() override;
		void Update();
	private:
		void MakeEnemy();
		float EnemySpawnTimer = 10.0f;
		std::vector<ObjPtr<GameObject>> Enemys;
	};
}