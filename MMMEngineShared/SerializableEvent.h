#pragma once

#include "Export.h"
#include "MUID.h"
#include "Object.h"
#include "Behaviour.h"
#include <vector>
#include <string>
#include <functional>

namespace MMMEngine
{
	/// 직렬화 가능한 한 개의 "호출 슬롯": 대상(MUID) + 메시지 이름.
	/// 씬 저장 시 MUID/이름만 저장하고, 로드 후 Invoke 시 리졸버로 대상 Behaviour를 찾아 CallMessage 호출.
	struct MMMENGINE_API PersistentCall
	{
		std::string targetMUID;  /// 대상 컴포넌트(Behaviour) MUID 문자열
		std::string messageName; /// CallMessage에 넘길 메시지 이름

		PersistentCall() = default;
		PersistentCall(std::string muid, std::string name)
			: targetMUID(std::move(muid)), messageName(std::move(name)) {}

		const std::string& GetTargetMUID() const { return targetMUID; }
		void SetTargetMUID(const std::string& s) { targetMUID = s; }
		const std::string& GetMessageName() const { return messageName; }
		void SetMessageName(const std::string& s) { messageName = s; }
	};

	/// 직렬화 가능한 void() 이벤트. Button::onClick 등에 사용.
	/// PersistentCall 목록을 저장/복원하고, Invoke() 시 리졸버로 대상 Behaviour를 찾아 CallMessage(name) 호출.
	class MMMENGINE_API SerializableEvent
	{
	public:
		using Resolver = std::function<ObjPtr<Object>(const Utility::MUID&)>;

		SerializableEvent() = default;

		/// MUID->Object 리졸버 설정. 씬 로드 후 엔진에서 호출 (예: Scene의 객체 테이블로 설정).
		static void SetResolver(Resolver resolver) { s_resolver = std::move(resolver); }
		static void ClearResolver() { s_resolver = nullptr; }

		void AddPersistentCall(const Utility::MUID& targetMUID, const std::string& messageName);
		void AddPersistentCall(const std::string& targetMUIDStr, const std::string& messageName);
		void RemovePersistentCall(size_t index);
		size_t GetPersistentCallCount() const { return m_calls.size(); }
		const PersistentCall& GetPersistentCall(size_t index) const { return m_calls[index]; }

		/// 직렬화/역직렬화용: 호출 목록 접근
		const std::vector<PersistentCall>& GetCalls() const { return m_calls; }
		void SetCalls(const std::vector<PersistentCall>& calls) { m_calls = calls; }

		void Invoke();

	private:
		static Resolver s_resolver;
		std::vector<PersistentCall> m_calls;
	};

	/// 직렬화 가능한 void(T) 이벤트. HandleGage::onValueChanged 등에 사용.
	template<typename T>
	class SerializableEventT
	{
	public:
		using Resolver = std::function<ObjPtr<Object>(const Utility::MUID&)>;

		SerializableEventT() = default;

		static void SetResolver(Resolver resolver) { s_resolver = std::move(resolver); }
		static void ClearResolver() { s_resolver = nullptr; }

		void AddPersistentCall(const Utility::MUID& targetMUID, const std::string& messageName);
		void AddPersistentCall(const std::string& targetMUIDStr, const std::string& messageName);
		void RemovePersistentCall(size_t index);
		size_t GetPersistentCallCount() const { return m_calls.size(); }
		const PersistentCall& GetPersistentCall(size_t index) const { return m_calls[index]; }

		const std::vector<PersistentCall>& GetCalls() const { return m_calls; }
		void SetCalls(const std::vector<PersistentCall>& calls) { m_calls = calls; }

		void Invoke(T arg);

	private:
		static Resolver s_resolver;
		std::vector<PersistentCall> m_calls;
	};

	// --- template 구현 ---
	template<typename T>
	typename SerializableEventT<T>::Resolver SerializableEventT<T>::s_resolver;

	template<typename T>
	void SerializableEventT<T>::AddPersistentCall(const Utility::MUID& targetMUID, const std::string& messageName)
	{
		m_calls.emplace_back(targetMUID.ToString(), messageName);
	}

	template<typename T>
	void SerializableEventT<T>::AddPersistentCall(const std::string& targetMUIDStr, const std::string& messageName)
	{
		m_calls.emplace_back(targetMUIDStr, messageName);
	}

	template<typename T>
	void SerializableEventT<T>::RemovePersistentCall(size_t index)
	{
		if (index < m_calls.size())
		{
			m_calls.erase(m_calls.begin() + static_cast<std::ptrdiff_t>(index));
		}
	}

	template<typename T>
	void SerializableEventT<T>::Invoke(T arg)
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
			ObjPtr<Behaviour> behaviourPtr = obj.Cast<Behaviour>();
			if (behaviourPtr.IsValid())
				behaviourPtr->CallMessage(call.messageName, arg);
		}
	}
}
