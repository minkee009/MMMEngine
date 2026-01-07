#pragma once
#include "Object.h"
#include "rttr/type"
#include <vector>

namespace MMMEngine
{
	class Transform;
	class Component;
	class GameObject : public Object
	{
	private:
		RTTR_ENABLE(Object)
		RTTR_REGISTRATION_FRIEND
		ObjectPtr<Transform> m_transform;
		std::vector<ObjectPtr<Component>> m_components;
		friend class App;
		friend class ObjectManager;
		friend class Scene;

		void RegisterComponent(ObjectPtr<Component> comp);
		void UnRegisterComponent(ObjectPtr<Component> comp);

	protected:
		GameObject() = default;
		GameObject(std::string name);
	public:
		virtual ~GameObject() = default;
		
		template <typename T>
		ObjectPtr<T> AddComponent()
		{
			static_assert(!std::is_same_v<T, Transform>, "Transform은 Addcomponent로 생성할 수 없습니다.");
			static_assert(std::is_base_of_v<Component, T>, "T는 Component를 상속받아야 합니다.");

			auto newComponent = Object::CreatePtr<T>();
			newComponent->m_gameObject = SelfPtr(this);
			RegisterComponent(newComponent);

			//if constexpr (std::is_same<T, Canvas>::value || std::is_base_of<Graphic, T>::value)
			//{
			//	EnsureRectTransform();
			//	UpdateActiveInHierarchy();
			//}

			newComponent.Cast<Component>()->Initialize();

			return newComponent;
		}

		template <typename T>
		ObjectPtr<T> GetComponent()
		{
			static_assert(std::is_base_of<Component, T>::value, "GetComponent()의 T는 Component를 상속받아야 합니다.");

			for (auto comp : m_components)
			{
				if (auto typedComp = comp.Cast<T>())
					return typedComp;
			}

			return nullptr;
		}

		const std::vector<ObjectPtr<Component>>& GetAllComponents() const { return m_components; }

		ObjectPtr<Transform> GetTransform() { return m_transform; }
	};
}