#pragma once
#include "ScriptBehaviour.h"
#include "MMMApplication.h"

namespace MMMEngine {
	class Player : public ScriptBehaviour
	{
	public:
		void Initialize() override;
		void UnInitialize() override;
		void Update();
		void GetDamage(int t) { HP -= t; };
		void VelocityDown(float t) { velocity = bestvelocity - t; };
		void VelocityReturn() { velocity = 20.0f; };
		bool IsMoving() const{ return isMoving; }
	private:
		int HP = 100;
		float velocity = 20.0f;
		float bestvelocity = 20.0f;
		float attackTimer = 1.0f;
		ObjPtr<GameObject> targetEnemy = nullptr;
		bool isMoving = false;
	};
}