#include "Building.h"
#include "rttr/registration"
#include "rttr/detail/policies/ctor_policies.h"

RTTR_PLUGIN_REGISTRATION
{
	using namespace rttr;
	using namespace MMMEngine;

	registration::class_<Building>("Building")
		(rttr::metadata("wrapper_type", rttr::type::get<ObjPtr<Building>>()));

	registration::class_<ObjPtr<Building>>("ObjPtr<Building>")
		.constructor(
			[]() {
				return Object::NewObject<Building>();
			});

	type::register_wrapper_converter_for_base_classes<MMMEngine::ObjPtr<Building>>();
}

void MMMEngine::Building::Start()
{

}

void MMMEngine::Building::Initialize()
{

}

void MMMEngine::Building::UnInitialize()
{

}

void MMMEngine::Building::Update()
{
	if (HP <= 0)
		Destroy(GetGameObject());
}

void MMMEngine::Building::PointUp(float t)
{
	auto p = static_cast<int>(t);
	point += p;
	exp += 10 * p;
}