#include "Enemy.h"
#include "Player.h"
#include "MMMTime.h"
#include "Transform.h"

void MMMEngine::Enemy::Initialize()
{
	player = GameObject::Find("Player");
}

void MMMEngine::Enemy::Update()
{
	auto pc = player->GetComponent<Player>();
	auto playerpos = player->GetTransform()->GetWorldPosition();
	auto tr = GetTransform();
	auto pos = tr->GetWorldPosition();
	if (!playerFind)
	{
		//성 좌표, 나중에 성 좌표계로 수정
		float targetX = 0.0f;
		float targetZ = 0.0f;

		float dx = targetX - pos.x;
		float dz = targetZ - pos.z;

		float dist = std::sqrt(dx * dx + dz * dz);
		if (dist > 0.01f)
		{
			dx /= dist;
			dz /= dist;

			pos.x += dx * velocity * Time::GetDeltaTime();
			pos.z += dz * velocity * Time::GetDeltaTime();
			tr->SetWorldPosition(pos);
		}
		else
		{
			pos.x = targetX;
			pos.z = targetZ;
		}
		auto fwd3 = tr->GetWorldMatrix().Forward();   // 또는 WorldMatrix().Forward()
		fwd3.y = 0.0f;

		fwd3.Normalize();

		// 2) Enemy -> Player (XZ)
		float pldx = playerpos.x - pos.x;
		float pldz = playerpos.z - pos.z;

		float dist2 = pldx * pldx + pldz * pldz;

		float dist = sqrtf(dist2);
		float invDist = 1.0f / dist;
		pldx *= invDist;
		pldz *= invDist;

		// 3) dot = cos(theta)
		float dot = fwd3.x * pldx + fwd3.z * pldz;

		// 4) 시야각(예: cos(60deg)=0.5) OR 거리
		if (dot >= 0.5f || dist < 10.0f)
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
				pc->GetDamage(10);
				attackTimer = 1.0f;
			}
		}
		if (pldist > 100.f)
			playerFind = false;
	}
	if(HP<=0)
		Destroy(GetGameObject());
}