#pragma once
#include "ScriptBehaviour.h"
#include "MMMApplication.h"

namespace MMMEngine {
	class Player : public ScriptBehaviour
	{
	public:
		void Initialize() {};
		void Update();
		void Damage(int t) { HP -= t; };
		float posX = 0.0f;
		float posY = 0.0f;
	private:
		int HP = 100;
		float invincibleTimer = 0.0f;
		float velocity = 200.0f;
	};
}