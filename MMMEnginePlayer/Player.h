#pragma once
#include "ScriptBehaviour.h"
#include <DirectXMath.h>
#include <SimpleMath.h>

namespace MMMEngine {
	class Transform;
	class SnowballManager;
	class Player : public ScriptBehaviour
	{
	public:
		void Initialize() override;
		void UnInitialize() override;
		void Update();
		void GetDamage(int t);
		void VelocityDown(float t) { velocity = std::max(10.f - t, 6.5f); };
		void VelocityReturn() { velocity = bestvelocity; };
		bool IsScoopMoving() const{ return scoopHeld&&isMoving; }
		bool PlayerDeath() const { return HP <= 0; }
		bool AttachSnowball(ObjPtr<GameObject> snow);
		void DetachSnowball();
		void SnapToSnowball();
		float GetPickupRange() const { return pickupRange; }
		ObjPtr<GameObject> GetMatchedSnowball()const { return matchedSnowball; }
	private:
		void HandleMovement();
		void HandleTargeting();
		void HandleAttack();
		void ClearTarget();
		void UpdateScoop();
		void AutoHeal();
		int HP = 100; //플레이어 체력
		int prevHP = 100;
		int maxHP = 100;
		//지금 스탯은 눈필드 기준, 필드관련 매니저 코드에 상태 추가 후 입력받아 스탯 변경
		float velocity = 13.0f; //플레이어 속도
		float bestvelocity = 13.0f; // 플레이어 기본 속도
		float yawRad = 0.0f;                         // 현재 yaw
		float turnSpeedRad = DirectX::XM_PI * 2.0f;  // 360deg/s (원하는 값으로 튜닝)
		float battledist = 3.5f; //플레이어 적 공격 거리
		int atk = 10; //플레이어 공격력
		float attackTimer = 0.0f;
		float attackDelay = 0.65f; //플레이어 공격 간격
		float pickupRange = 1.0f; //눈 픽업 거리
		float offset = 1.5f; //눈과 플레이어간의 거리
		float healTimer = 0.0f;
		float healDelay = 1.0f;
		float nonfightTimer = 0.0f;
		float nonfightDelay = 10.0f;
		bool fighting = false;
		int healHP = 10;
		float damageTimer = 0.0f;
		float damageDelay = 0.1f;
		ObjPtr<GameObject> matchedSnowball = nullptr;
		ObjPtr<GameObject> targetEnemy = nullptr;
		float offset = 1.5f; //눈과 플레이어간의 거리
		bool isMoving = false;
		bool scoopHeld = false;
		ObjPtr<Transform> tr;
		DirectX::SimpleMath::Vector3 pos;
		DirectX::SimpleMath::Quaternion rot;
	};
}