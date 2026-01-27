#include "Enemy.h"
#include "Player.h"
#include "Castle.h"
#include "Building.h"
#include "MMMTime.h"
#include "Transform.h"
#include "rttr/registration"
#include "rttr/detail/policies/ctor_policies.h"

RTTR_PLUGIN_REGISTRATION
{
	using namespace rttr;
	using namespace MMMEngine;

	registration::class_<Enemy>("Enemy")
		(rttr::metadata("wrapper_type", rttr::type::get<ObjPtr<Enemy>>()));

	registration::class_<ObjPtr<Enemy>>("ObjPtr<Enemy>")
		.constructor(
			[]() {
				return Object::NewObject<Enemy>();
			});

	type::register_wrapper_converter_for_base_classes<MMMEngine::ObjPtr<Enemy>>();
}

void MMMEngine::Enemy::Start()
{

}

void MMMEngine::Enemy::Initialize()
{
	tr = GetTransform();

	player = GameObject::Find("Player");
	if (player) {
		playercomp = player->GetComponent<Player>();
		playertr = player->GetTransform();
	}

	castle = GameObject::Find("Castle");
	if (castle) {
		castlecomp = castle->GetComponent<Castle>();
		castletr = castle->GetTransform();
	}
	Configure();
}

void MMMEngine::Enemy::UnInitialize()
{
	player = nullptr;
	castle = nullptr;
}

void MMMEngine::Enemy::Update()
{
	if (!tr || !playertr || !castletr || !playercomp || !castlecomp) return;
	pos = tr->GetWorldPosition();
	playerpos = playertr->GetWorldPosition();
	castlepos = castletr->GetWorldPosition();

	if (HP <= 0) {
		Destroy(GetGameObject());
		return;
	}

	if (attackCastle)
	{
		attackTimer += Time::GetDeltaTime();
		if (attackTimer >= attackDelay)
		{
			castlecomp->GetDamage(atk);
			attackTimer = 0.0f;
		}
		return;
	}

	if (playerFind)
	{
		if (LostPlayer())
			return;
		bool movetoplayer = MoveToTarget(playerpos, battledist);
		if (movetoplayer)
		{
			attackTimer = 0.0f;
		}
		else
		{
			attackTimer += Time::GetDeltaTime();
			if (attackTimer >= attackDelay)
			{
				playercomp->GetDamage(atk);
				attackTimer = 0.0f;
			}
		}
		return;
	}
	
	bool movetocastle = MoveToTarget(castlepos, battledist);
	if (!movetocastle)
	{
		attackCastle = true;
		attackTimer = 0.0f;
		return;
	}
	CheckPlayer();
}

bool MMMEngine::Enemy::MoveToTarget(const DirectX::SimpleMath::Vector3 target, float stopDist)
{
	float dx = target.x - pos.x;
	float dz = target.z - pos.z;

	float dist2 = dx * dx + dz * dz;
	if (dist2 <= stopDist * stopDist)
		return false; // 도착(또는 충분히 가까움)

	float dist = sqrtf(dist2);
	dx /= dist; dz /= dist;

	pos.x += dx * velocity * Time::GetDeltaTime();
	pos.z += dz * velocity * Time::GetDeltaTime();
	tr->SetWorldPosition(pos);

	float yaw = atan2f(dx, dz);
	auto rot = DirectX::SimpleMath::Quaternion::CreateFromYawPitchRoll(yaw, 0, 0);
	tr->SetWorldRotation(rot);
	return true;
}

bool MMMEngine::Enemy::LostPlayer()
{
	float dx = playerpos.x - pos.x;
	float dz = playerpos.z - pos.z;
	float dist2 = dx * dx + dz * dz;

	if (dist2 > playerLostdist * playerLostdist) {
		playerFind = false;
		attackTimer = 0.0f;
		return true;
	}
	return false;
}

void MMMEngine::Enemy::CheckPlayer()
{
	auto fwd = DirectX::SimpleMath::Vector3::Transform(
		DirectX::SimpleMath::Vector3(0, 0, 1),
		tr->GetWorldRotation()
	);
	fwd.y = 0.0f;
	if (fwd.LengthSquared() < 1e-8f) return;
	fwd.Normalize();

	float vx = playerpos.x - pos.x;
	float vz = playerpos.z - pos.z;
	float pd2 = vx * vx + vz * vz;
	if (pd2 < playercheckdist * playercheckdist && pd2 > 1e-8f)
	{
		float inv = 1.0f / sqrtf(pd2);
		vx *= inv; vz *= inv;
		float dot = fwd.x * vx + fwd.z * vz;

		if (dot >= 0.5f)
		{
			playerFind = true;
		}
	}
}

void MMMEngine::Enemy::PlayerHitMe()
{
	playerFind = true;
}