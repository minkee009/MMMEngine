#pragma once
#include "ColliderComponent.h"
#include "StaticMesh.h"
#include "RigidBodyComponent.h"

namespace MMMEngine
{
	class MMMENGINE_API MeshColliderComponent : public ColliderComponent
	{
	private:
		RTTR_ENABLE(ColliderComponent)
			RTTR_REGISTRATION_FRIEND
	public:
		enum class MeshMode { Auto, Triangle, Convex };
		void SetMesh(ResPtr<StaticMesh> mesh);
		ResPtr<StaticMesh> GetMesh();

		void SetSubmesh(int idx); // 선택 사항

		bool BuildShape(physx::PxPhysics* physics, physx::PxMaterial* material) override;

		void Initialize() override;

		// 외부에서 호출해 shape 생성 + 물리 등록까지 수행
		bool TryBuildAndRegister();

		bool UpdateShapeGeometry() override;

		DebugColliderShapeDesc GetDebugShapeDesc() const override { return {}; }

		bool RebuildForRigidType(MMMEngine::RigidBodyComponent::Type type);
		bool RebuildShapeOnly(MMMEngine::RigidBodyComponent::Type type);





	private:
		ResPtr<StaticMesh> m_mesh;
		int m_submesh = -1; // -1이면 전체 합치기 or 첫 submesh
		MeshMode m_mode = MeshMode::Triangle;

		physx::PxTriangleMesh* m_tri = nullptr;
		physx::PxConvexMesh* m_convex = nullptr;

	private:
		bool ExtractMeshData(const StaticMesh& mesh, int submesh, std::vector<physx::PxVec3>& outVerts, std::vector<uint32_t>& outIndices);
		physx::PxTriangleMesh* CookTriangle(const std::vector<physx::PxVec3>& verts, const std::vector<uint32_t>& indices);
		physx::PxConvexMesh* CookConvex(const std::vector<physx::PxVec3>& verts);
		bool IsDynamicTarget();
	};
}

