#pragma once
#include "ScriptBehaviour.h"
#include "MMMApplication.h"

namespace MMMEngine {
	class Enemy : public ScriptBehaviour
	{
	public:
		void Initialize() override;
		void Update();
		void GetDamage(int t) { HP -= t; };
		void PlayerHitMe() { playerFind = true; };
		float posX = 100.0f;
		float posY = 100.0f;
	private:
		int HP = 50;
		bool playerFind = false;
		float forwardAngle = 0.0f;
		float velocity = 50.0f;
		float attackTimer = 1.0f;
		ObjPtr<GameObject> player;
	};
}

