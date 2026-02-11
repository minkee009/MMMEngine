#pragma once
#include "ColliderComponent.h"

namespace MMMEngine
{
	class MMMENGINE_API BoxColliderComponent : public ColliderComponent
	{
	private:
		RTTR_ENABLE(ColliderComponent)
		RTTR_REGISTRATION_FRIEND
	public:
		void SetHalfExtents(Vector3 he);
		Vector3 GetHalfExtents() const;

		bool UpdateShapeGeometry() override;

		void PrintFilter() override;

		bool BuildShape(physx::PxPhysics* physics, physx::PxMaterial* material) override;

		DebugColliderShapeDesc GetDebugShapeDesc() const override;
	private:
		Vector3 m_halfExtents = { 0.5f, 0.5f, 0.5f };
	};
}
