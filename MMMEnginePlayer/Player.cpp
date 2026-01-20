#include "Player.h"
#include "MMMInput.h"
#include "Enemy.h"
#include "MMMTime.h"

void MMMEngine::Player::Update()
{
	float dx = 0.0f;
	float dy = 0.0f;
	if (Input::GetKey(KeyCode::LeftArrow))  dx -= 1.0f;
	if (Input::GetKey(KeyCode::RightArrow)) dx += 1.0f;
	if (Input::GetKey(KeyCode::UpArrow))    dy += 1.0f;
	if (Input::GetKey(KeyCode::DownArrow))  dy -= 1.0f;
	isMoving = (dx != 0.0f || dy != 0.0f);
	posX += dx * velocity * Time::GetDeltaTime();
	posY += dy * velocity * Time::GetDeltaTime();
	float battledist = 5.0f;
	if (!targetEnemy) {
		auto enemy = GameObject::FindGameObjectsWithTag("Enemy");
		for (auto& e : enemy) {
			auto ec = e->GetComponent<Enemy>();
			float enemyX = ec->posX;
			float enemyY = ec->posY;
			float edx = enemyX - posX;
			float edy = enemyY - posY;
			float edist = sqrtf(edx * edx + edy * edy);

			if (edist < battledist) {
				targetEnemy = e;
				battledist = edist;
			}
		};
		attackTimer = 1.0f;
	}
	else
	{
		auto tec = targetEnemy->GetComponent<Enemy>();
		float enemyX = tec->posX;
		float enemyY = tec->posY;
		float edx = enemyX - posX;
		float edy = enemyY - posY;
		float edist = sqrtf(edx * edx + edy * edy);
		if (edist > 5.0f)
		{
			targetEnemy = nullptr;
			attackTimer = 1.0f;
			return;
		}
		attackTimer -= Time::GetDeltaTime();
		if (attackTimer <= 0.0f)
		{
			tec->GetDamage(10);
			tec->PlayerHitMe();
			attackTimer = 1.0f;
		}
	}
	if (HP <= 0) {
		Destroy(GetGameObject());
	}
}