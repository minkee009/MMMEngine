#pragma once
#include "ScriptBehaviour.h"

namespace MMMEngine {
	class Castle : public ScriptBehaviour
	{
	public:
		void Initialize() override;
		void UnInitialize() override;
		void Update();
		void CoinUp(float t);
		void GetDamage(int t) { HP -= t; };
		bool CastleDeath() const { return HP <= 0; }
	private:
		void AutoHeal();
		int HP = 10;
		int prevHP;
		int maxHP = 10;
		int coin = 0;
		bool fighting = false;
		float healTimer = 0.0f;
		float healDelay = 1.0f;
		float NonfightTimer = 0.0f;
		float NonfightDelay = 1.0f;
	};
}