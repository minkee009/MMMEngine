#include "MissingScriptBehaviour.h"

#include "rttr/registration"
#include "rttr/detail/policies/ctor_policies.h"

RTTR_REGISTRATION
{
	using namespace rttr;
	using namespace MMMEngine;

    registration::class_<MissingScriptBehaviour>("MissingScriptBehaviour")
        (metadata("INSPECTOR", "DONT_ADD_COMP"))
        (metadata("wrapper_type_name", "ObjPtr<MissingScriptBehaviour>"))
        .property("OriginalTypeName",
            &MissingScriptBehaviour::GetOriginalTypeName,
            &MissingScriptBehaviour::SetOriginalTypeName)(rttr::metadata("INSPECTOR", "HIDDEN"))
        .property_readonly("TypeName",
            &MissingScriptBehaviour::GetOriginalTypeName);

    registration::class_<ObjPtr<MissingScriptBehaviour>>("ObjPtr<MissingScriptBehaviour>")
        .constructor<>([]() { return Object::NewObject<MissingScriptBehaviour>(); });
}
