#pragma once
#include "ColliderComponent.h"

namespace MMMEngine
{
	class MMMENGINE_API CapsuleColliderComponent : public ColliderComponent
	{
	private:
		RTTR_ENABLE(Component)
		RTTR_REGISTRATION_FRIEND
	public:
		void SetRadius(float radius);
		void SetHalfHeight(float halfheight);

		float GetRadius() const;
		float GetHalfHeight() const;

		bool UpdateShapeGeometry() override;

		void ApplyLocalPose() override;

		void BuildShape(physx::PxPhysics* physics, physx::PxMaterial* material) override;

		DebugColliderShapeDesc GetDebugShapeDesc() const override;
	private:
		float m_radius = 0.5f;
		float m_halfHeight = 1.0f;

		physx::PxTransform m_AxisCorrection = physx::PxTransform(physx::PxQuat(physx::PxHalfPi, physx::PxVec3(0, 0, 1)));
	};
}

