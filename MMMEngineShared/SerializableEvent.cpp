#include "SerializableEvent.h"
#include "Behaviour.h"
#include "rttr/registration"

RTTR_REGISTRATION
{
	using namespace rttr;
	using namespace MMMEngine;

	registration::class_<PersistentCall>("PersistentCall")
		.property("TargetMUID", &PersistentCall::GetTargetMUID, &PersistentCall::SetTargetMUID)
		.property("MessageName", &PersistentCall::GetMessageName, &PersistentCall::SetMessageName);

	registration::class_<SerializableEvent>("SerializableEvent")
		.property("Calls", &SerializableEvent::GetCalls, &SerializableEvent::SetCalls);

	registration::class_<SerializableEventT<float>>("SerializableEventFloat")
		.property("Calls", &SerializableEventT<float>::GetCalls, &SerializableEventT<float>::SetCalls);

	registration::class_<SerializableEventT<bool>>("SerializableEventBool")
		.property("Calls", &SerializableEventT<bool>::GetCalls, &SerializableEventT<bool>::SetCalls);

	registration::class_<SerializableEventT<int>>("SerializableEventInt")
		.property("Calls", &SerializableEventT<int>::GetCalls, &SerializableEventT<int>::SetCalls);

	registration::class_<SerializableEventT<std::string>>("SerializableEventString")
		.property("Calls", &SerializableEventT<std::string>::GetCalls, &SerializableEventT<std::string>::SetCalls);
}

namespace MMMEngine
{
	SerializableEvent::Resolver SerializableEvent::s_resolver;

	void SerializableEvent::AddPersistentCall(const Utility::MUID& targetMUID, const std::string& messageName)
	{
		m_calls.emplace_back(targetMUID.ToString(), messageName);
	}

	void SerializableEvent::AddPersistentCall(const std::string& targetMUIDStr, const std::string& messageName)
	{
		m_calls.emplace_back(targetMUIDStr, messageName);
	}

	void SerializableEvent::RemovePersistentCall(size_t index)
	{
		if (index < m_calls.size())
			m_calls.erase(m_calls.begin() + static_cast<std::ptrdiff_t>(index));
	}

	void SerializableEvent::Invoke()
	{
		if (!s_resolver)
			return;
		for (const auto& call : m_calls)
		{
			auto parsed = Utility::MUID::Parse(call.targetMUID);
			if (!parsed.has_value())
				continue;
			ObjPtr<Object> obj = s_resolver(parsed.value());
			if (!obj.IsValid())
				continue;
			auto behaviour = obj.Cast<Behaviour>();
			if (behaviour.IsValid())
				behaviour->CallMessage(call.messageName);
		}
	}
}
