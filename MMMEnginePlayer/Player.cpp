#include "Player.h"
#include "MMMInput.h"
#include "Enemy.h"
#include "MMMTime.h"
#include "Transform.h"
#include "Snowball.h"
#include "SnowballManager.h"

static float WrapPi(float a)
{
	while (a > DirectX::XM_PI)  a -= DirectX::XM_2PI;
	while (a < -DirectX::XM_PI) a += DirectX::XM_2PI;
	return a;
}

// 기본은 '최단각'으로 돌되, 정확히 180도 근처면 오른쪽으로 강제
static float StepYaw(float current, float target, float maxStep)
{
	float diff = WrapPi(target - current); // [-pi, pi]

	// 180도 근처(반전)면 오른쪽(시계방향)으로 돌게 강제
	// 여기서 오른쪽을 +yaw로 정의했을 때: diff가 -pi 근처면 +2pi로 바꿔서 +pi로 만들어줌
	const float eps = 0.01f;
	if (fabsf(fabsf(diff) - DirectX::XM_PI) < eps)
	{
		if (diff < 0.0f) diff += DirectX::XM_2PI; // -pi -> +pi
	}

	// 최단각 회전(일반 케이스)
	float step = std::clamp(diff, -maxStep, +maxStep);
	return WrapPi(current + step);
}

void MMMEngine::Player::Initialize()
{
	tr = GetTransform();
	targetEnemy = nullptr;
	matchedSnowball = nullptr;
	auto fwd = tr->GetWorldMatrix().Forward(); // 보통 월드 기준 forward
	// fwd = (x, y, z)

	// +Z가 전방인 LH 기준 yaw 계산
	yawRad = atan2f(fwd.x, fwd.z);
	yawRad = WrapPi(yawRad);
}

void MMMEngine::Player::UnInitialize()
{
	if (targetEnemy)
		targetEnemy = nullptr;
	if (matchedSnowball)
		matchedSnowball = nullptr;
}

void MMMEngine::Player::Update()
{
	if (!tr) return;
	pos = tr->GetWorldPosition();
	rot = tr->GetWorldRotation();
	HandleMovement();
	UpdateScoop();
	if (matchedSnowball)
	{
		VelocityDown(matchedSnowball->GetComponent<Snowball>()->GetScale());
	}
	if (Input::GetKey(KeyCode::Space)) {
		//스페이스 누른상태면 공격불가
		ClearTarget();
		return;
	}
	HandleTargeting();
	HandleAttack();
}

void MMMEngine::Player::HandleMovement()
{
	float dx = 0.0f;
	float dz = 0.0f;
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

		float desiredYaw = atan2f(dx, dz); // 목표 yaw
		float maxStep = turnSpeedRad * Time::GetDeltaTime();

		yawRad = StepYaw(yawRad, desiredYaw, maxStep);

		auto rot = DirectX::SimpleMath::Quaternion::CreateFromYawPitchRoll(yawRad, 0.0f, 0.0f);
		tr->SetWorldRotation(rot);
	}
}

void MMMEngine::Player::HandleTargeting()
{

	if (targetEnemy)
	{
		auto tect = targetEnemy->GetTransform();
		float edx = tect->GetWorldPosition().x - pos.x;
		float edz = tect->GetWorldPosition().z - pos.z;
		float edist = sqrtf(edx * edx + edz * edz);

		if (edist > battledist) {
			ClearTarget();
		}
		return;
	}

	auto enemy = GameObject::FindGameObjectsWithTag("Enemy");
	float bestdist = battledist;
	for (auto& e : enemy) {
		auto enemypos = e->GetTransform()->GetWorldPosition();
		float edx = enemypos.x - pos.x;
		float edz = enemypos.z - pos.z;
		float edist = sqrtf(edx * edx + edz * edz);
		if (edist < bestdist) {
			targetEnemy = e;
			bestdist = edist;
		}
	}
}

void MMMEngine::Player::HandleAttack()
{
	if (!targetEnemy) return;
	auto tec = targetEnemy->GetComponent<Enemy>();
	if (!tec) { ClearTarget(); return; }
	//애니메이션이 아직 없어서 별도의 타이머로 동작. 공격모션 추가 후 수정
	attackTimer += Time::GetDeltaTime();
	if (attackTimer >= attackDelay)
	{
		tec->GetDamage(atk);
		tec->PlayerHitMe();
		attackTimer = 0.0f;
	}
}

void MMMEngine::Player::ClearTarget()
{
	targetEnemy = nullptr;
	attackTimer = 0.0f;
}

bool MMMEngine::Player::AttachSnowball(ObjPtr<GameObject> snow)
{
	if (!snow) return false;

	if (matchedSnowball == snow) return true;
	if (matchedSnowball) DetachSnowball();

	matchedSnowball = snow;
	return true;
}

void MMMEngine::Player::DetachSnowball()
{
	if (!matchedSnowball) return;

	matchedSnowball = nullptr;
	VelocityReturn();
}

void MMMEngine::Player::UpdateScoop()
{
	if (Input::GetKeyDown(KeyCode::Space)) {
		SnowballManager::instance->OnScoopStart(*this);
		scoopHeld = true;
	}
	if (Input::GetKey(KeyCode::Space)) {
		SnowballManager::instance->OnScoopHold(*this);
	}
	if (Input::GetKeyUp(KeyCode::Space)) {
		SnowballManager::instance->OnScoopEnd(*this);
		scoopHeld = false;
	}
}