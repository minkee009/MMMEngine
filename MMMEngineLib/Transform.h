#pragma once
#include "Component.h"
#include "rttr/type"
#include "SimpleMath.h"
#include "Delegates.hpp"

using namespace DirectX::SimpleMath;

namespace MMMEngine
{
	class Transform : public Component
	{
	private:
		RTTR_ENABLE(Component)
		RTTR_REGISTRATION_FRIEND
		friend class App;
		friend class ObjectManager;
		friend class GameObject;
		
		Vector3 m_localPosition;
		Quaternion m_localRotation;
		Vector3 m_localScale;

		mutable bool m_isLocalMatDirty;
		mutable bool m_isWorldMatDirty;
		mutable Matrix m_cachedLocalMat;
		mutable Matrix m_cachedWorldMat;

		ObjectPtr<Transform> m_parent;
		std::vector<ObjectPtr<Transform>> m_childs;

		void AddChild(ObjectPtr<Transform> child);
		void RemoveChild(ObjectPtr<Transform> child);
		void MarkDirty();
	protected:
		Transform();
		virtual void Initialize() override {};
	public:
		virtual ~Transform();

		const Matrix& GetLocalMatrix() const;
		const Matrix& GetWorldMatrix() const;

		const Vector3& GetLocalPosition() const;
		const Quaternion& GetLocalRotation() const;
		const Vector3 GetLocalEulerRotation() const; // return degree (0 ~ 360)
		const Vector3& GetLocalScale() const;

		const Vector3 GetWorldPosition() const;
		const Quaternion GetWorldRotation() const;
		const Vector3 GetWorldEulerRotation() const; // return degree (0 ~ 360)
		const Vector3 GetWorldScale() const;

		ObjectPtr<Transform> GetChild(size_t index);

		void SetWorldPosition(Vector3 pos);
		inline void SetWorldPosition(float x, float y, float z) { SetWorldPosition({ x,y,z }); }

		void SetWorldRotation(Quaternion rot);
		inline void SetWorldRotation(float x, float y, float z, float w) { SetWorldRotation({ x,y,z,w }); }

		void SetWorldEulerRotation(Vector3 rot); // use degree (0 ~ 360)
		inline void SetWorldEulerRotation(float x, float y, float z) { SetWorldEulerRotation({ x,y,z }); } // use degree (0 ~ 360)

		void SetWorldScale(Vector3 scale);
		inline void SetWorldScale(float x, float y, float z) { SetWorldScale({ x,y,z }); }

		void SetLocalPosition(Vector3 pos);
		inline void SetLocalPosition(float x, float y, float z) { SetLocalPosition({ x,y,z }); }

		void SetLocalRotation(Quaternion rot);
		inline void SetLocalRotation(float x, float y, float z, float w) { SetLocalRotation({ x,y,z,w }); }

		void SetParent(ObjectPtr<Transform> parent, bool worldPositionStays = true);
		
		inline ObjectPtr<Transform> GetParent() const { return m_parent; }

		Event<Transform, void(void)> onMatrixUpdate{ this };
	};
}