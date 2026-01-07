#include "GameObject.h"
#include "rttr/registration"
#include "rttr/detail/policies/ctor_policies.h"
#include "Component.h"
#include "Transform.h"
#include <cmath>

RTTR_REGISTRATION
{
	using namespace rttr;
	using namespace MMMEngine;

    registration::class_<GameObject>("GameObject");

	registration::class_<ObjectPtr<GameObject>>("ObjectPtr<GameObject>")
		.constructor(
   			[](const std::string& name) {
   				return Object::CreatePtr<GameObject>(name);
   			})
   	    .constructor<>(
   		    []() {
   			    return Object::CreatePtr<GameObject>();
   		    });
}

void MMMEngine::GameObject::RegisterComponent(ObjectPtr<Component> comp)
{
}

void MMMEngine::GameObject::UnRegisterComponent(ObjectPtr<Component> comp)
{
}

MMMEngine::GameObject::GameObject(std::string name)
{
	SetName(name);
}
