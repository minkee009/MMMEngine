#include "Snowball.h"
#include "MMMTime.h"
#include "Player.h"
#include "Transform.h"
#include "rttr/registration"
#include "rttr/detail/policies/ctor_policies.h"

RTTR_PLUGIN_REGISTRATION
{
	using namespace rttr;
	using namespace MMMEngine;

	registration::class_<Player>("Snowball")
		(rttr::metadata("wrapper_type", rttr::type::get<ObjPtr<Snowball>>()));

	registration::class_<ObjPtr<Snowball>>("ObjPtr<Snowball>")
		.constructor(
			[]() {
				return Object::NewObject<Snowball>();
			});

	type::register_wrapper_converter_for_base_classes<MMMEngine::ObjPtr<Snowball>>();
}

void Start()
{

}

void MMMEngine::Snowball::Initialize()
{
	tr = GetTransform();
}

void MMMEngine::Snowball::UnInitialize()
{

}

void MMMEngine::Snowball::Update()
{
	if (IsCarried())
	{
		pos = tr->GetWorldPosition();
		auto trPlayer = carrier->GetTransform();
		playerpos = trPlayer->GetWorldPosition();
		playerrot = trPlayer->GetWorldRotation();
		RollSnow();
	}

	DirectX::SimpleMath::Vector3 scaleVector = { scale, scale, scale };
	tr->SetWorldScale(scaleVector);
}

void MMMEngine::Snowball::RollSnow()
{
	//지금은 플레이어가 잡고 이동만 해도 사이즈가 커짐, 필드 시스템 추후 추가
	if(scale <= maxscale && carrier->IsScoopMoving())
		scale = std::min(scale + Time::GetDeltaTime(), maxscale);
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

void MMMEngine::Snowball::EatSnow(ObjPtr<GameObject> other)
{
	if (!other || other == GetGameObject()) return;
	auto snowcomp = other->GetComponent<Snowball>();
	if (snowcomp == nullptr)
		return;
	if (snowcomp->scale > maxscale - scale)
		scale = maxscale;
	else
		scale += snowcomp->scale;
	Destroy(other);
}