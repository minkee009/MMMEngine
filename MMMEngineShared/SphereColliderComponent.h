#pragma once
#include "ColliderComponent.h"

namespace MMMEngine
{
	class MMMENGINE_API SphereColliderComponent : public ColliderComponent
	{
	private:
		RTTR_ENABLE(ColliderComponent)
		RTTR_REGISTRATION_FRIEND
	public:
		void SetRadius(float radius);

		float GetRadius() const;

		bool UpdateShapeGeometry() override;

		bool BuildShape(physx::PxPhysics* physics, physx::PxMaterial* material) override;

		DebugColliderShapeDesc GetDebugShapeDesc() const override;
	private:
		float m_radius = 0.5f;
	};
}


