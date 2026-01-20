#include "Enemy.h"
#include "Player.h"
#include "MMMTime.h"

void MMMEngine::Enemy::Initialize()
{
	player = GameObject::Find("Player");
}

void MMMEngine::Enemy::Update()
{
	auto pc = player->GetComponent<Player>();
	if (!playerFind)
	{
		float targetX = 0.0f;
		float targetY = 0.0f;

		float dx = targetX - posX;
		float dy = targetY - posY;

		float dist = std::sqrt(dx * dx + dy * dy);
		if (dist > 0.01f)
		{
			dx /= dist;
			dy /= dist;

			posX += dx * velocity * Time::GetDeltaTime();
			posY += dy * velocity * Time::GetDeltaTime();
		}
		else
		{
			posX = targetX;
			posY = targetY;
		}

		float playerX = pc->posX;
		float playerY = pc->posY;
		float forwardX = cosf(forwardAngle);
		float forwardY = sinf(forwardAngle);

		// Enemy -> Player
		float pldx = playerX - posX;
		float pldy = playerY - posY;

		float pldist = sqrtf(pldx * pldx + pldy * pldy);
		if (pldist <= 0.0001f)
			return;

		pldx /= pldist;
		pldy /= pldist;

		float dot = forwardX * pldx + forwardY * pldy;

		if (dot >= 0.5f || pldist < 100.0f)
		{
			playerFind = true;
			attackTimer = 1.0f;
		}
	}
	else
	{
		float playerX = pc->posX;
		float playerY = pc->posY;

		float pldx = playerX - posX;
		float pldy = playerY - posY;
		float pldist = sqrtf(pldx * pldx + pldy * pldy);

		if (pldist > 5.0f)
		{
			pldx /= pldist;
			pldy /= pldist;

			posX += pldx * velocity * Time::GetDeltaTime();
			posY += pldy * velocity * Time::GetDeltaTime();
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