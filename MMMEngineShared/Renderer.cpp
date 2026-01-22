#include "Renderer.h"
#include "RenderManager.h"
#include "GameObject.h"

#include "rttr/registration"
#include "rttr/detail/policies/ctor_policies.h"

RTTR_REGISTRATION
{
	using namespace rttr;
	using namespace MMMEngine;

	registration::class_<Renderer>("Renderer")
		.property("Enabled", &Renderer::GetEnabled, &Renderer::SetEnabled)
		.property("RenderPriority", &Renderer::GetRenderPriority, &Renderer::SetRenderPriority);

	//registration::class_<ObjPtr<Renderer>>("ObjPtr<Renderer>");

	type::register_wrapper_converter_for_base_classes<MMMEngine::ObjPtr<Renderer>>();
}

void MMMEngine::Renderer::Initialize()
{
	RenderManager::Get().RegisterRenderer(SelfPtr(this));
}

void MMMEngine::Renderer::UnInitialize()
{
	RenderManager::Get().UnRegisterRenderer(SelfPtr(this));
}

void MMMEngine::Renderer::SetEnabled(bool enabled)
{
	auto lastValue = m_enabled;
	m_enabled = enabled;

	//렌더러에 보낼 메시지 혹은 마킹
}

int MMMEngine::Renderer::GetRenderPriority()
{
	return m_renderPriority;
}

void MMMEngine::Renderer::SetRenderPriority(int priority)
{
	m_renderPriority = priority;
}

bool MMMEngine::Renderer::GetEnabled()
{
	return m_enabled 
		&& GetGameObject().IsValid()
		&& GetGameObject()->IsActiveInHierarchy();
}
