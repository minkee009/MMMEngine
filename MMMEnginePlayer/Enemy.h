#pragma once
#include "ScriptBehaviour.h"
#include "MMMApplication.h"

namespace MMMEngine {
	class Enemy : public ScriptBehaviour
	{
	public:
		void Initialize() override;
		void UnInitialize() override;
		void Update();
		void GetDamage(int t) { HP -= t; };
		void PlayerHitMe();
	private:
		int HP = 50;
		bool playerFind = false;
		float velocity = 10.0f;
		float attackTimer = 1.0f;
		bool attackCastle = false;
		ObjPtr<GameObject> player;
		ObjPtr<GameObject> castle;
	};
}

