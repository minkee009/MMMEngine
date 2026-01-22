#include "Player.h"
#include "MMMInput.h"
#include "Enemy.h"
#include "MMMTime.h"
#include "Transform.h"

void MMMEngine::Player::Initialize()
{

}

void MMMEngine::Player::UnInitialize()
{

}

void MMMEngine::Player::Update()
{
	float dx = 0.0f;
	float dz = 0.0f;
	auto tr = GetTransform();
	auto pos = GetTransform()->GetWorldPosition();
	if (Input::GetKey(KeyCode::LeftArrow))  dx -= 1.0f;
	if (Input::GetKey(KeyCode::RightArrow)) dx += 1.0f;
	if (Input::GetKey(KeyCode::UpArrow))    dz += 1.0f;
	if (Input::GetKey(KeyCode::DownArrow))  dz -= 1.0f;
	isMoving = (dx != 0.0f || dz != 0.0f);
	if (isMoving) {
		float len = sqrtf(dx * dx + dz * dz);
		dx /= len;
		dz /= len;

		pos.x += dx * velocity * Time::GetDeltaTime();
		pos.z += dz * velocity * Time::GetDeltaTime();
		tr->SetWorldPosition(pos);

		// 회전: +Z가 Forward인 LH 기준 yaw
		float yaw = atan2f(dx, dz); // 라디안
		auto rot = DirectX::SimpleMath::Quaternion::CreateFromYawPitchRoll(yaw, 0.0f, 0.0f);
		tr->SetWorldRotation(rot);
	}
	if (Input::GetKey(KeyCode::Space)) {
		targetEnemy = nullptr;
		attackTimer = 1.0f;
		return;
	}
	float battledist = 5.0f;
	if (!targetEnemy) {
		auto enemy = GameObject::FindGameObjectsWithTag("Enemy");
		for (auto& e : enemy) {
			auto enemypos = e->GetTransform()->GetWorldPosition();
			float enemyX = enemypos.x;
			float enemyZ = enemypos.z;
			float edx = enemyX - pos.x;
			float edz = enemyZ - pos.z;
			float edist = sqrtf(edx * edx + edz * edz);

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
		auto tect = targetEnemy->GetTransform();
		float enemyX = tect->GetWorldPosition().x;
		float enemyZ = tect->GetWorldPosition().z;
		float edx = enemyX - pos.x;
		float edz = enemyZ - pos.z;
		float edist = sqrtf(edx * edx + edz * edz);
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
}