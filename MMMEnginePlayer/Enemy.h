#pragma once
#include "ScriptBehaviour.h"
#include <SimpleMath.h>

namespace MMMEngine {
	class Transform;
	class Player;
	class Castle;
	class Enemy : public ScriptBehaviour
	{
	public:
		void Initialize() override;
		void UnInitialize() override;
		void Update();
		void GetDamage(int t) { HP -= t; };
		void PlayerHitMe();
	private:
		bool MoveToTarget(const DirectX::SimpleMath::Vector3 target, float stopDist);
		void CheckPlayer();
		bool LostPlayer();
		int HP = 50; //적 HP
		int atk = 3; //적 공격력
		bool playerFind = false;
		float playerLostdist = 50.0f;
		float playercheckdist = 10.0f;
		float velocity = 10.0f;
		float attackDelay = 1.0f;
		float attackTimer = 0.0f;
		float battledist = 5.0f;
		bool attackCastle = false;
		ObjPtr<Transform> tr;
		DirectX::SimpleMath::Vector3 pos;
		ObjPtr<GameObject> player;
		ObjPtr<Player> playercomp;
		ObjPtr<Transform> playertr;
		DirectX::SimpleMath::Vector3 playerpos;
		ObjPtr<GameObject> castle;
		ObjPtr<Castle> castlecomp;
		ObjPtr<Transform> castletr;
		DirectX::SimpleMath::Vector3 castlepos;
	};
}

