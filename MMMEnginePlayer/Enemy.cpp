#include "Enemy.h"
#include "Player.h"
#include "Castle.h"
#include "MMMTime.h"
#include "Transform.h"

void MMMEngine::Enemy::Initialize()
{
	player = GameObject::Find("Player");
	castle = GameObject::Find("Castle");
}

void MMMEngine::Enemy::UnInitialize()
{
	player = nullptr;
	castle = nullptr;
}

void MMMEngine::Enemy::Update()
{
	auto playercomp = player->GetComponent<Player>();
	auto playerpos = player->GetTransform()->GetWorldPosition();
	auto castlecomp = castle->GetComponent<Castle>();
	auto castlepos = castle->GetTransform()->GetWorldPosition();
	auto tr = GetTransform();
	auto pos = tr->GetWorldPosition();
	if (!playerFind)
	{
		//타겟 설정은 이후 건물이 추가되면 성을 포함한 건물들 중 가장 가까운 건물로 설정 추가
		float targetX = castlepos.x;
		float targetZ = castlepos.z;

		float dx = targetX - pos.x;
		float dz = targetZ - pos.z;

		float dist = std::sqrt(dx * dx + dz * dz);
		if (dist > 1.f)
		{
			dx /= dist;
			dz /= dist;

			pos.x += dx * velocity * Time::GetDeltaTime();
			pos.z += dz * velocity * Time::GetDeltaTime();
		}
		
		else
		{
			attackCastle = true;
			attackTimer = 1.0f;
		}
		tr->SetWorldPosition(pos);

		if (attackCastle)
		{
			attackTimer -= Time::GetDeltaTime();
			if (attackTimer <= 0.0f)
			{
				castlecomp->GetDamage(1);
				attackTimer = 1.0f;
			}
		}

		//Enemy정면 계산, 실제 테스트 및 transform 설정에 따라 수정 해야함
		auto fwd3 = tr->GetWorldMatrix().Forward();
		fwd3.y = 0.0f;

		fwd3.Normalize();

		// Enemy -> Player (XZ)
		float pldx = playerpos.x - pos.x;
		float pldz = playerpos.z - pos.z;

		float dist2 = sqrtf(pldx * pldx + pldz * pldz);

		float invDist = 1.0f / dist2;
		pldx *= invDist;
		pldz *= invDist;

		// dot = cos(theta)
		float dot = fwd3.x * pldx + fwd3.z * pldz;

		// 시야각(예: cos(60deg)=0.5) And 거리 And 성 공격 상태 아님
		if (dot >= 0.5f && dist < 10.0f && !attackCastle)
		{
			playerFind = true;
			attackTimer = 1.0f;
		}
	}
	else
	{
		float pldx = playerpos.x - pos.x;
		float pldz = playerpos.z - pos.z;
		float pldist = sqrtf(pldx * pldx + pldz * pldz);

		if (pldist > 5.0f)
		{
			pldx /= pldist;
			pldz /= pldist;

			pos.x += pldx * velocity * Time::GetDeltaTime();
			pos.z += pldz * velocity * Time::GetDeltaTime();
			tr->SetWorldPosition(pos);
			attackTimer = 1.0f;
		}
		else
		{
			attackTimer -= Time::GetDeltaTime();
			if (attackTimer <= 0.0f)
			{
				playercomp->GetDamage(10);
				attackTimer = 1.0f;
			}
		}
		if (pldist > 100.f)
			playerFind = false;
	}
	if(HP<=0)
		Destroy(GetGameObject());
}

void MMMEngine::Enemy::PlayerHitMe()
{
	if (!attackCastle) {
		playerFind = true;
		attackTimer = 1.0f;
	}
}