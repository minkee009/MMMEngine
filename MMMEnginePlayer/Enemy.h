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
	private:
		int HP = 50;
		bool playerFind = false;
		float velocity = 10.0f;
		float attackTimer = 1.0f;
		ObjPtr<GameObject> player;
	};
}

