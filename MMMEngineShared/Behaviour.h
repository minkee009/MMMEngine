#pragma once
#include "Component.h"
#include "GameObject.h"
#include "Export.h"
#include <string>
#include <unordered_map>
#include <memory>
#include <stdexcept>
#include <any>
#include <typeinfo>
#include <vector>

#pragma warning(push)
#pragma warning(disable: 4251)  // STL ??? ????

#define REGISTER_BEHAVIOUR_MESSAGE(func) RegisterMessage(#func, &std::remove_reference_t<decltype(*this)>::func);

namespace MMMEngine
{
	// ???? ????
	class Behaviour;

	// === ????? ????? ===
	struct MMMENGINE_API BehaviourMessageBase
	{
		virtual ~BehaviourMessageBase() = default;
		virtual void InvokeRaw(void** args) = 0;
		virtual void InvokeVoid() = 0;
		virtual size_t GetArgCount() const = 0;
		virtual const std::type_info* GetArgType(size_t index) const = 0;
	};

	template<typename T>
	struct BehaviourMessage : BehaviourMessageBase
	{
		using FuncType = void(T::*)();

		Behaviour* owner;
		FuncType func;

		BehaviourMessage(Behaviour* owner, FuncType func)
			: owner(owner), func(func) {
		}

		void InvokeVoid() override
		{
			(static_cast<T*>(owner)->*func)();
		}

		void InvokeRaw(void**) override
		{
			return;
			//throw std::runtime_error("This message does not accept parameters.");
		}

		size_t GetArgCount() const override
		{
			return 0;
		}

		const std::type_info* GetArgType(size_t) const override
		{
			return nullptr;
		}
	};

	template<typename T, typename... Args>
	struct BehaviourParamMessage : BehaviourMessageBase
	{
		using FuncType = void(T::*)(Args...);

		Behaviour* owner;
		FuncType func;

		BehaviourParamMessage(Behaviour* owner, FuncType func)
			: owner(owner), func(func) {
		}

		void InvokeRaw(void** args) override
		{
			InvokeImpl(args, std::index_sequence_for<Args...>{});
		}

		void InvokeVoid() override
		{
			return;
			//throw std::runtime_error("This message requires parameters.");
		}

		size_t GetArgCount() const override
		{
			return sizeof...(Args);
		}

		const std::type_info* GetArgType(size_t index) const override
		{
			return GetArgTypeImpl(index, std::index_sequence_for<Args...>{});
		}

	private:
		template<std::size_t... I>
		const std::type_info* GetArgTypeImpl(size_t index, std::index_sequence<I...>) const
		{
			const std::type_info* types[] = { &typeid(Args)... };
			if (index >= sizeof...(Args))
				return nullptr;
			return types[index];
		}

		template<std::size_t... I>
		void InvokeImpl(void** args, std::index_sequence<I...>)
		{
			(static_cast<T*>(owner)->*func)(*reinterpret_cast<typename std::tuple_element<I, std::tuple<Args...>>::type*>(args[I])...);
		}
	};


	//???? ???? ?????? ???????????.
	class MMMENGINE_API Behaviour : public Component
	{
	private:
		RTTR_ENABLE(Component)
		RTTR_REGISTRATION_FRIEND
		friend class BehaviourManager;
		friend class GameObject;

		std::unordered_map<std::string, std::unique_ptr<BehaviourMessageBase>> m_messages;

	protected:
		Behaviour();
		virtual void Initialize() override;
		virtual void UnInitialize() override;
		void OnOwnerActiveInHierarchyChanged();

		bool m_enabled;
		int m_executionOrder = 0; // ???? ?????? ??????? ????

		void SetExecutionOrder(int order) { m_executionOrder = order; }
	
		template<typename T>
		void RegisterMessage(const std::string& name, void(T::* func)())
		{
			m_messages[name] = std::make_unique<BehaviourMessage<T>>(this, func);
		}

		template<typename T, typename... Args>
		void RegisterMessage(const std::string& name, void(T::* func)(Args...))
		{
			m_messages[name] = std::make_unique<BehaviourParamMessage<T, Args...>>(this, func);
		}
		

	public:
		virtual ~Behaviour() = default;

		bool GetEnabled() const { return m_enabled; }
		void SetEnabled(bool value);

		bool IsActiveAndEnabled();

		/// ??? ??? ???? ?? (?? ??). SerializableEvent ??? ??.
		void CallMessage(const std::string& name)
		{
			auto it = m_messages.find(name);
			if (it == m_messages.end())
				return;
			it->second->InvokeVoid();
		}

		/// ??? ??? ???? ?? (?? ??). SerializableEvent ??? ??.
		template<typename... Args>
		void CallMessage(const std::string& name, Args&&... args)
		{
			void* argArray[] = { (void*)&args... };
			auto it = m_messages.find(name);
			if (it == m_messages.end())
				return;
			it->second->InvokeRaw(argArray);
		}

		void GetVoidMessageNames(std::vector<std::string>& outNames) const;
		void GetFloatMessageNames(std::vector<std::string>& outNames) const;
		void GetBoolMessageNames(std::vector<std::string>& outNames) const;
		void GetIntMessageNames(std::vector<std::string>& outNames) const;
		void GetStringMessageNames(std::vector<std::string>& outNames) const;
	};
}
