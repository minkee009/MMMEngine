#pragma once
#include "rttr/registration"
#include "rttr/detail/policies/ctor_policies.h"

using namespace rttr;

namespace MMMEngine
{
	RTTR_REGISTRATION
	{
		registration::class_<Object>("Object")
			.constructor<>()
				(rttr::policy::ctor::as_raw_ptr)
			.property("name", Object::GetName, Object::SetName);
	}

	class Object
	{
	private:
		static uint64_t s_nextInstanceID;
		uint64_t		m_instanceID;
		bool			m_isDestroyed = false;

		void MarkDestory() { m_isDestroyed = true; }
	protected:
		std::string m_name;

		Object() : m_instanceID(s_nextInstanceID++)
		{
			m_name = "<Unnamed> [ Instance ID : " + std::to_string(m_instanceID) + " ]";
		}

		virtual ~Object() = default;

	public:
		inline uint64_t				GetInstanceID() const { return m_instanceID; }
		inline const std::string&	GetName()		const { return m_name; }

		inline bool					IsDestroyed()	const { return m_isDestroyed; }

		inline void					SetName(const std::string& name) { m_name = name; }
	};

}