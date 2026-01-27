#include "Castle.h"
#include "MMMTime.h"
#include "rttr/registration"
#include "rttr/detail/policies/ctor_policies.h"

RTTR_PLUGIN_REGISTRATION
{
	using namespace rttr;
	using namespace MMMEngine;

	registration::class_<Castle>("Castle")
		(rttr::metadata("wrapper_type", rttr::type::get<ObjPtr<Castle>>()));

	registration::class_<ObjPtr<Castle>>("ObjPtr<Castle>")
		.constructor(
			[]() {
				return Object::NewObject<Castle>();
			});

	type::register_wrapper_converter_for_base_classes<MMMEngine::ObjPtr<Castle>>();
}

void MMMEngine::Castle::Start()
{

}

void MMMEngine::Castle::Initialize()
{

}

void MMMEngine::Castle::UnInitialize()
{

}

void MMMEngine::Castle::Update()
{
	AutoHeal();
}

void MMMEngine::Castle::AutoHeal()
{
	if (prevHP > HP)
	{
		fighting = true;
		NonfightTimer = 0.0f;
	}
	prevHP = HP;
	if (fighting)
	{
		NonfightTimer += Time::GetDeltaTime();
		if (NonfightTimer >= NonfightDelay)
		{
			fighting = false;
			healTimer = 0.0f;
		}
	}
	else if (HP < maxHP)
	{
		healTimer += Time::GetDeltaTime();
		if (healTimer >= healDelay)
		{
			HP = std::min(HP + healHP, maxHP);
			healTimer = 0.0f;
		}
	}
}

void MMMEngine::Castle::PointUp(float t)
{
	auto p = static_cast<int>(t);
	point += p;
	exp += 10 * p;
}