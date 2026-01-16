#include "Component.h"
#include "GameObject.h"
#include "Transform.h"

#include "rttr/registration"
#include "rttr/detail/policies/ctor_policies.h"

RTTR_REGISTRATION
{
	using namespace rttr;
	using namespace MMMEngine;

	registration::class_<Component>("Component")
		.property("GameObject", &Component::GetGameObject, &Component::SetGameObject, registration::private_access)
		.property_readonly("Transform", &Component::GetTransform);

	//registration::class_<ObjPtr<Component>>("ObjPtr<Component>");

	type::register_wrapper_converter_for_base_classes<MMMEngine::ObjPtr<Component>>();
}


void MMMEngine::Component::Dispose()
{
	if(GetGameObject().IsValid() && !GetGameObject()->IsDestroyed())
		GetGameObject()->UnRegisterComponent(SelfPtr(this));
	UnInitialize();
}

MMMEngine::ObjPtr<MMMEngine::Transform> MMMEngine::Component::GetTransform()
{
	if(m_gameObject.IsValid())
		return m_gameObject->GetTransform();

	return nullptr;
}
