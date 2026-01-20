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
		ObjPtr<GameObject> player;
		ObjPtr<GameObject> matchedSnowball = nullptr;

		float spawnTimer = 0.0f;
		float pickupRange = 5.0f;
		float spawnDelay = 0.2f;
		std::vector<ObjPtr<GameObject>> Snows;
	};
}

