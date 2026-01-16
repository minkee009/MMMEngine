#include "MissingScriptBehaviour.h"

#include "rttr/registration"
#include "rttr/detail/policies/ctor_policies.h"

RTTR_REGISTRATION
{
	using namespace rttr;
	using namespace MMMEngine;

	registration::class_<MissingScriptBehaviour>("MissingScriptBehaviour");

	registration::class_<ObjPtr<MissingScriptBehaviour>>("ObjPtr<MissingScriptBehaviour>")
		.constructor<>(
			[]() {
				return Object::NewObject<MissingScriptBehaviour>();
			});


	type::register_wrapper_converter_for_base_classes<MMMEngine::ObjPtr<MissingScriptBehaviour>>();
}
