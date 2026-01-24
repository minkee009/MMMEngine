#include "ListenerComponent.h"
#include "Transform.h"

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