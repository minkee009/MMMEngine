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
			tr->SetWorldPosition(pos);

			// 회전 (이동 방향 바라보기)
			float yaw = atan2f(dx, dz); // LH(+Z forward)
			auto rot = DirectX::SimpleMath::Quaternion::CreateFromYawPitchRoll(yaw, 0.0f, 0.0f);
			tr->SetWorldRotation(rot);
		}
		else
		{
			attackCastle = true;
			attackTimer = 1.0f;
		}

		if (attackCastle)
		{
			attackTimer -= Time::GetDeltaTime();
			if (attackTimer <= 0.0f)
			{
				castlecomp->GetDamage(1);
				attackTimer = 1.0f;
			}
		}

		// Enemy forward (월드 회전 기준)
		auto fwd3 = DirectX::SimpleMath::Vector3::Transform(
			DirectX::SimpleMath::Vector3::Forward,
			tr->GetWorldRotation()
		);
		fwd3.y = 0.0f;
		if (fwd3.LengthSquared() < 1e-8f) return;
		fwd3.Normalize();

		// Enemy -> Player (XZ)
		float pldx = playerpos.x - pos.x;
		float pldz = playerpos.z - pos.z;

		float playerDist2 = pldx * pldx + pldz * pldz;
		if (playerDist2 < 1e-8f) return;

		float playerDist = sqrtf(playerDist2);
		float invDist = 1.0f / playerDist;
		pldx *= invDist;
		pldz *= invDist;

		// dot
		float dot = fwd3.x * pldx + fwd3.z * pldz;

		// 시야각 + 거리 + 성 공격 중 아님
		if (dot >= 0.5f && playerDist < 10.0f && !attackCastle)
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

			// 회전 (이동 방향 바라보기)
			float yaw = atan2f(pldx, pldz); // LH(+Z forward)
			auto rot = DirectX::SimpleMath::Quaternion::CreateFromYawPitchRoll(yaw, 0.0f, 0.0f);
			tr->SetWorldRotation(rot);
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
		if (pldist > 50.f)
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