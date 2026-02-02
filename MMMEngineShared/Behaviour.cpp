#include "Behaviour.h"
#include "BehaviourManager.h"

#include "rttr/registration"
#include "rttr/detail/policies/ctor_policies.h"

RTTR_REGISTRATION
{
	using namespace rttr;
	using namespace MMMEngine;

	registration::class_<Behaviour>("Behaviour")
		(rttr::metadata("INSPECTOR", "DONT_ADD_COMP"))
		.property("Enabled", &Behaviour::GetEnabled, &Behaviour::SetEnabled)
		.property_readonly("IsActiveAndEnabled", &Behaviour::IsActiveAndEnabled)(rttr::metadata("INSPECTOR", "HIDDEN"));
}


MMMEngine::Behaviour::Behaviour() 
	: m_enabled(true)
{
	
}

void MMMEngine::Behaviour::Initialize()
{
	BehaviourManager::Get().RegisterBehaviour(SelfPtr(this));
}

void MMMEngine::Behaviour::UnInitialize()
{
	BehaviourManager::Get().UnRegisterBehaviour(SelfPtr(this)); // BehaviourManager에서 제거
	m_messages.clear();
}

void MMMEngine::Behaviour::SetEnabled(bool value)
{
	if (value != m_enabled)
	{
		//바뀔 때 무언갈 실행하는 코드 작성하기
		m_enabled = value;
	}
}

bool MMMEngine::Behaviour::IsActiveAndEnabled()
{
	return m_enabled && GetGameObject().IsValid() && GetGameObject()->IsActiveInHierarchy();
}

void MMMEngine::Behaviour::GetVoidMessageNames(std::vector<std::string>& outNames) const
{
	outNames.clear();
	outNames.reserve(m_messages.size());

	for (const auto& entry : m_messages)
	{
		if (!entry.second)
			continue;

		if (entry.second->GetArgCount() == 0)
			outNames.push_back(entry.first);
	}
}

void MMMEngine::Behaviour::GetFloatMessageNames(std::vector<std::string>& outNames) const
{
	outNames.clear();
	outNames.reserve(m_messages.size());

	for (const auto& entry : m_messages)
	{
		if (!entry.second)
			continue;

		if (entry.second->GetArgCount() != 1)
			continue;

		const std::type_info* argType = entry.second->GetArgType(0);
		if (argType && (*argType == typeid(float)))
			outNames.push_back(entry.first);
	}
}
