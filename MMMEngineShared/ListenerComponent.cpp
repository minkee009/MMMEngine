#include "ListenerComponent.h"
#include "Transform.h"
#include "rttr/registration.h"

RTTR_REGISTRATION
{
	using namespace rttr;
	using namespace MMMEngine;

	registration::class_<ListenerComponent>("ListenerComponent")
		(rttr::metadata("wrapper_type_name", "ObjPtr<ListenerComponent>"));
	registration::class_<ObjPtr<ListenerComponent>>("ObjPtr<ListenerComponent>")
		.constructor<>(
			[]() {
				return Object::NewObject<ListenerComponent>();
			});
}
void MMMEngine::ListenerComponent::Update()
{
	auto transform = GetTransform();
	pos = transform->GetWorldPosition();
	fwd = transform->GetWorldMatrix().Forward();
	up = transform->GetWorldMatrix().Up();
	AudioManager::Get().SetListenerPosition(pos.x, pos.y, pos.z,
		fwd.x, fwd.y, fwd.z,
		up.x, up.y, up.z);
}
