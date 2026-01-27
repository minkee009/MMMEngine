#include "NormalEnemy.h"
#include "rttr/registration"
#include "rttr/detail/policies/ctor_policies.h"

RTTR_PLUGIN_REGISTRATION
{
	using namespace rttr;
	using namespace MMMEngine;

	registration::class_<NormalEnemy>("NormalEnemy")
		(rttr::metadata("wrapper_type", rttr::type::get<ObjPtr<NormalEnemy>>()));

	registration::class_<ObjPtr<NormalEnemy>>("ObjPtr<NormalEnemy>")
		.constructor(
			[]() {
				return Object::NewObject<NormalEnemy>();
			});

	type::register_wrapper_converter_for_base_classes<MMMEngine::ObjPtr<NormalEnemy>>();
}