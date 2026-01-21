#pragma once
#include "ScriptBehaviour.h"
#include "MMMApplication.h"

namespace MMMEngine {
	class Castle : public ScriptBehaviour
	{
	public:
		void Initialize() override {};
		void UnInitialize() override {};
		void Update();
		void CoinUp(float t);
		float posX = 0.0f;
		float posY = 0.0f;
	private:
		int HP = 10;
		int coin = 0;
		bool fighting = false;
		float healTimer = 1.0f;
	};
}