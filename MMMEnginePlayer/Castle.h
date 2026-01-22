#pragma once
#include "ScriptBehaviour.h"
#include "MMMApplication.h"

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
		int HP = 10;
		int prevHP;
		int coin = 0;
		bool fighting = false;
		float healTimer = 1.0f;
		float NonfightTimer = 5.0f;
	};
}