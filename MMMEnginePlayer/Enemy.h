#pragma once
#include "ScriptBehaviour.h"
#include <SimpleMath.h>
#include "Export.h"
#include "rttr/type"

namespace MMMEngine {
	class Transform;
	class Player;
	class Castle;
	class MMMENGINE_API Enemy : public ScriptBehaviour
	{
		RTTR_ENABLE(ScriptBehaviour)
		RTTR_REGISTRATION_FRIEND
	public:
		Enemy()
		{
			REGISTER_BEHAVIOUR_MESSAGE(Start)
			REGISTER_BEHAVIOUR_MESSAGE(Update)
		}
		void Initialize() override;
		void UnInitialize() override;
		void Start();
		void Update();
		void GetDamage(int t) { HP - t; HP = std::max(HP, 0); };
		void PlayerHitMe();
	private:
		bool MoveToTarget(const DirectX::SimpleMath::Vector3 target, float stopDist);
		void CheckPlayer();
		bool LostPlayer();
	protected:
		virtual void Configure() {};
		int HP = 50; //적 HP
		int atk = 3; //적 공격력
		bool playerFind = false;
		float playerLostdist = 50.0f;
		float playercheckdist = 10.0f;
		float velocity = 13.0f;
		float attackDelay = 0.65f;
		float attackTimer = 0.0f;
		float battledist = 1.7f;
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

