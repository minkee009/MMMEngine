#include "Enemy.h"
#include "MMMTime.h"

void MMMEngine::Enemy::Update()
{
	auto player = GameObject::Find("player");
	float dt = Time::GetDeltaTime();
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

			posX += dx * velocity * dt;
			posY += dy * velocity * dt;
		}
		else
		{
			posX = targetX;
			posY = targetY;
		}

		float playerX = player->GetComponent<Player>()->posX;
		float playerY = player->GetComponent<Player>()->posY;
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
		float playerX = player->GetComponent<Player>()->posX;
		float playerY = player->GetComponent<Player>()->posY;

		float pldx = playerX - posX;
		float pldy = playerY - posY;
		float pldist = sqrtf(pldx * pldx + pldy * pldy);

		if (pldist > 5.0f)
		{
			pldx /= pldist;
			pldy /= pldist;

			posX += pldx* velocity* dt;
			posY += pldy * velocity * dt;
			attackTimer = 1.0f;
		}
		else
		{
			attackTimer -= dt;
			if (attackTimer <= 0.0f)
			{
				player->GetComponent<Player>()->Damage(10);
				attackTimer = 1.0f;
			}
		}
		if (pldist > 100.f)
			playerFind = false;
	}
}