#include "Snowball.h"
#include "MMMTime.h"
#include "Player.h"
#include "Transform.h"

void MMMEngine::Snowball::Initialize()
{
	player = GameObject::Find("Player");
}

void MMMEngine::Snowball::UnInitialize()
{
	player = nullptr;
}

void MMMEngine::Snowball::Update()
{
	if (controlled)
		RollSnow();
}

void MMMEngine::Snowball::RollSnow()
{
	//지금은 플레이어가 잡고 이동만 해도 사이즈가 커짐, 필드 시스템 추후 추가
	if(scale <= 10.0f && player->GetComponent<Player>()->IsMoving())
		scale += Time::GetDeltaTime();
	auto tr = GetTransform();
	auto pos = GetTransform()->GetWorldPosition();
	auto pTr = player->GetTransform();
	auto playerpos = pTr->GetWorldPosition();
	auto playerrot = pTr->GetWorldRotation();
	auto fwd = DirectX::SimpleMath::Vector3::Transform(
		DirectX::SimpleMath::Vector3::Forward, playerrot);
	fwd.y = 0.0f;
	if (fwd.LengthSquared() > 1e-8f) fwd.Normalize();
	auto target = playerpos + fwd * offset;
	auto to = target - pos;
	to.y = 0.0f;

	float dist = to.Length();
	if (dist > 1e-3f)
	{
		auto dir = to / dist;
		float step = velocity * Time::GetDeltaTime();
		if (step >= dist) pos = target;
		else pos += dir * step;

		tr->SetWorldPosition(pos);
	}
}

void MMMEngine::Snowball::AssembleSnow(ObjPtr<GameObject> other)
{
	auto snowcomp = other->GetComponent<Snowball>();
	if (snowcomp == nullptr)
		return;
	if (snowcomp->scale > 10.0f - scale)
		scale = 10.0f;
	else
		scale += snowcomp->scale;
	Destroy(other);
}