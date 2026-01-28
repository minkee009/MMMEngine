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
		.property_readonly("GameObject", &Component::GetGameObject, registration::private_access)
		.property_readonly("Transform", &Component::GetTransform)(rttr::metadata("INSPECTOR", "HIDDEN"));

	registration::class_<ObjPtr<Component>>("ObjPtr<Component>")
		.method("Inject", &ObjPtr<Component>::Inject);
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
