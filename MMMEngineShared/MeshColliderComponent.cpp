#include "pch.h"
#include "MeshColliderComponent.h"
#include "rttr/registration"
#include "PhysxManager.h"
#include "PhysxHelper.h"
#include "Transform.h"
#include "MeshRenderer.h"


RTTR_REGISTRATION
{
    using namespace rttr;
    using namespace MMMEngine;

    registration::class_<MeshColliderComponent>("MeshCollider")
        (rttr::metadata("wrapper_type_name", "ObjPtr<MeshColliderComponent>"))
        .property("Mesh", &MeshColliderComponent::GetMesh, &MeshColliderComponent::SetMesh)
        ;

    registration::class_<ObjPtr<MeshColliderComponent>>("ObjPtr<MeshColliderComponent>")
        .constructor(
            []() {
                return Object::NewObject<MeshColliderComponent>();
            })
        .method("Inject", &ObjPtr<MeshColliderComponent>::Inject);
}


void MMMEngine::MeshColliderComponent::SetMesh(ResPtr<StaticMesh> mesh)
{
    if (m_mesh == mesh) return;
    m_mesh = mesh;
    MarkGeometryDirty();
    TryBuildAndRegister();
}

MMMEngine::ResPtr<MMMEngine::StaticMesh> MMMEngine::MeshColliderComponent::GetMesh()
{
    return m_mesh;
}

void MMMEngine::MeshColliderComponent::SetSubmesh(int idx)
{
    if (m_submesh == idx) return;
    m_submesh = idx;
    MarkGeometryDirty();
}

bool MMMEngine::MeshColliderComponent::BuildShape(physx::PxPhysics* physics, physx::PxMaterial* material)
{
    if (!m_mesh || !physics || !material) return false;

    bool wantConvex = (m_mode == MeshMode::Convex);
    if (m_mode == MeshMode::Auto)
        wantConvex = IsDynamicTarget();
    else if (m_mode == MeshMode::Triangle)
    {
        // Dynamic이면 Triangle 금지 -> Convex로 강제
        if (IsDynamicTarget())
            wantConvex = true;
    }

    // meshData 추출
    std::vector<physx::PxVec3> verts;
    std::vector<uint32_t> indices;
    if (!ExtractMeshData(*m_mesh, m_submesh, verts, indices))
        return false;


    if (wantConvex)
    {
        if (m_tri) { m_tri->release(); m_tri = nullptr; }
        m_convex = CookConvex(verts);
        if (!m_convex) return false;
    }
    else
    {
        if (m_convex) { m_convex->release(); m_convex = nullptr; }
        m_tri = CookTriangle(verts, indices);
        if (!m_tri) return false;
    }

    physx::PxShape* shape = wantConvex
        ? physics->createShape(physx::PxConvexMeshGeometry(m_convex), *material, true)
        : physics->createShape(physx::PxTriangleMeshGeometry(m_tri), *material, true);

    const Vector3 World_Scale = GetTransform()->GetWorldScale();
    physx::PxMeshScale meshScale(physx::PxVec3(
        fabs(World_Scale.x), fabs(World_Scale.y), fabs(World_Scale.z)
    ));


    if (wantConvex)
    {
        physx::PxConvexMeshGeometry geom(m_convex, meshScale);
        if (!geom.isValid()) return false;
        shape = physics->createShape(geom, *material, true);
    }
    else
    {
        physx::PxTriangleMeshGeometry geom(m_tri, meshScale);
        if (!geom.isValid()) return false;
        shape = physics->createShape(geom, *material, true);
    }


    if (!shape) return false;
    SetShape(shape, true);
    return true;
}

void MMMEngine::MeshColliderComponent::Initialize()
{

    if (auto& obj = GetGameObject(); obj.IsValid())
    {
        obj->onActiveInHierarchyChanged
            .AddListener<ColliderComponent, &ColliderComponent::OnOwnerActiveInHierarchyChanged>(this);
    }


    if (GetGameObject().IsValid())
    {
        auto _meshRenderer = GetComponent<MeshRenderer>();
        if (_meshRenderer.IsValid())
        {
            m_mesh = _meshRenderer->GetMesh();
            TryBuildAndRegister();
        }
    }
}

bool MMMEngine::MeshColliderComponent::TryBuildAndRegister()
{
    if (!m_mesh)
    {
        auto mr = GetComponent<MeshRenderer>();
        if (mr.IsValid()) m_mesh = mr->GetMesh();
    }

    if (!m_mesh) return false;


    auto& physics = MMMEngine::PhysicX::Get().GetPhysics();
    EnsureMaterial();
    physx::PxMaterial* mat = m_Material ? m_Material : MMMEngine::PhysicX::Get().GetDefaultMaterial();
    if (!mat) return false;

    if (!m_Shape)
    {
        if (!BuildShape(&physics, mat) || !m_Shape) 
        {
            std::cout << u8"생성실패" << std::endl;
            return false;
        }
        RegisterToPhysics();
        return true;
    }
    UpdateShapeGeometry();

    if (!m_IsRegistered) RegisterToPhysics();
    else { std::cout << u8"생성실패2" << std::endl; }
    return true;
}

bool MMMEngine::MeshColliderComponent::UpdateShapeGeometry()
{
    if (!m_Shape) return false;

    const Vector3 ws = GetTransform()->GetWorldScale();
    physx::PxMeshScale meshScale(physx::PxVec3(
        fabs(ws.x), fabs(ws.y), fabs(ws.z)
    ));

    physx::PxGeometryHolder holder = m_Shape->getGeometry();
    switch (holder.getType())
    {
    case physx::PxGeometryType::eTRIANGLEMESH:
    {
        auto geom = holder.triangleMesh();
        geom.scale = meshScale;
        if (!geom.isValid()) return false;
        m_Shape->setGeometry(geom);
        ApplyAll();
        return true;
    }
    case physx::PxGeometryType::eCONVEXMESH:
    {
        auto geom = holder.convexMesh();
        geom.scale = meshScale;
        if (!geom.isValid()) return false;
        m_Shape->setGeometry(geom);
        ApplyAll();
        return true;
    }
    default:
        return false;
    }
}

bool MMMEngine::MeshColliderComponent::RebuildForRigidType(MMMEngine::RigidBodyComponent::Type type)
{
    if (!m_mesh) return false;

    // 원하는 타입 결정
    const bool wantConvex = (type == RigidBodyComponent::Type::Dynamic);

    auto& physics = MMMEngine::PhysicX::Get().GetPhysics();
    EnsureMaterial();
    auto* mat = m_Material ? m_Material : MMMEngine::PhysicX::Get().GetDefaultMaterial();
    if (!mat) return false;

    const bool wasRegistered = m_IsRegistered;

    if (wasRegistered)
        UnregisterFromPhysics(PhysicsUnregisterReason::Disable);

    // 기존 shape/mesh 정리
    if (m_Shape) { m_Shape->release(); m_Shape = nullptr; }
    if (m_tri) { m_tri->release(); m_tri = nullptr; }
    if (m_convex) { m_convex->release(); m_convex = nullptr; }

    if (!BuildShape(&physics, mat) || !m_Shape)
        return false;

    if (wasRegistered)
        RegisterToPhysics();

    return true;
}

bool MMMEngine::MeshColliderComponent::RebuildShapeOnly(MMMEngine::RigidBodyComponent::Type type)
{
    if (!m_mesh) return false;

    auto& physics = MMMEngine::PhysicX::Get().GetPhysics();
    EnsureMaterial();
    auto* mat = m_Material ? m_Material : MMMEngine::PhysicX::Get().GetDefaultMaterial();
    if (!mat) return false;

    // cooked mesh 정리 (누수 방지)
    if (m_tri) { m_tri->release(); m_tri = nullptr; }
    if (m_convex) { m_convex->release(); m_convex = nullptr; }

    // 전달된 type을 강제로 반영 (BuildShape가 IsDynamicTarget으로 덮어쓰지 않게)
    MeshMode prev = m_mode;
    m_mode = (type == RigidBodyComponent::Type::Dynamic) ? MeshMode::Convex : MeshMode::Triangle;

    const bool ok = BuildShape(&physics, mat);

    m_mode = prev;
    return ok;
}

bool MMMEngine::MeshColliderComponent::ExtractMeshData(const StaticMesh& mesh, int submesh, std::vector<physx::PxVec3>& outVerts, std::vector<uint32_t>& outIndices)
{
    outVerts.clear();
    outIndices.clear();

    const auto& vtx = mesh.meshData.vertices;
    const auto& idx = mesh.meshData.indices;
    if (vtx.empty() || idx.empty())
        return false;

    if (submesh >= 0)
    {
        if (submesh >= (int)vtx.size() || submesh >= (int)idx.size()) return false;
        const auto& v = vtx[submesh];
        const auto& i = idx[submesh];
        if (v.empty() || i.size() < 3 || (i.size() % 3) != 0) return false;

        outVerts.reserve(v.size());
        for (const auto& vert : v)
            outVerts.emplace_back(vert.Pos.x, vert.Pos.y, vert.Pos.z);

        outIndices.reserve(i.size());
        for (auto id : i)
            outIndices.push_back(static_cast<uint32_t>(id));

        return true;
    }

    // 전체 합치기 (-1)
    size_t totalVerts = 0, totalIndices = 0;
    for (size_t s = 0; s < vtx.size(); ++s)
    {
        if (vtx[s].empty() || idx[s].empty()) continue;
        totalVerts += vtx[s].size();
        totalIndices += idx[s].size();
    }
    if (totalVerts == 0 || totalIndices < 3 || (totalIndices % 3) != 0) return false;

    outVerts.reserve(totalVerts);
    outIndices.reserve(totalIndices);

    uint32_t base = 0;
    for (size_t s = 0; s < vtx.size(); ++s)
    {
        const auto& v = vtx[s];
        const auto& i = idx[s];
        if (v.empty() || i.empty()) continue;

        for (const auto& vert : v)
            outVerts.emplace_back(vert.Pos.x, vert.Pos.y, vert.Pos.z);

        for (auto id : i)
            outIndices.push_back(static_cast<uint32_t>(id) + base);

        base += static_cast<uint32_t>(v.size());
    }
    return true;
}

physx::PxTriangleMesh* MMMEngine::MeshColliderComponent::CookTriangle(const std::vector<physx::PxVec3>& verts, const std::vector<uint32_t>& indices)
{
    if (verts.empty() || indices.size() < 3 || (indices.size() % 3) != 0)
        return nullptr;

    physx::PxTriangleMeshDesc desc;
    desc.points.count = static_cast<physx::PxU32>(verts.size());
    desc.points.stride = sizeof(physx::PxVec3);
    desc.points.data = verts.data();

    desc.triangles.count = static_cast<physx::PxU32>(indices.size() / 3);
    desc.triangles.stride = sizeof(uint32_t) * 3;
    desc.triangles.data = indices.data();

    if (!desc.isValid())
        return nullptr;

    auto& cook = MMMEngine::PhysicX::Get().GetCookParam();
    return PxCreateTriangleMesh(cook, desc);
}

physx::PxConvexMesh* MMMEngine::MeshColliderComponent::CookConvex(const std::vector<physx::PxVec3>& verts)
{
    if (verts.size() < 4)
        return nullptr;

    physx::PxConvexMeshDesc desc;
    desc.points.count = static_cast<physx::PxU32>(verts.size());
    desc.points.stride = sizeof(physx::PxVec3);
    desc.points.data = verts.data();
    desc.flags = physx::PxConvexFlag::eCOMPUTE_CONVEX;
    // desc.vertexLimit = 256; // 필요하면 제한

    if (!desc.isValid())
        return nullptr;

    auto& cook = MMMEngine::PhysicX::Get().GetCookParam();
    return PxCreateConvexMesh(cook, desc);
}

bool MMMEngine::MeshColliderComponent::IsDynamicTarget()
{
    if (m_Shape)
    {
        if (auto* actor = m_Shape->getActor())
            return actor->is<physx::PxRigidDynamic>() != nullptr;
    }

    ObjPtr<RigidBodyComponent> last = nullptr;
    for (auto tr = GetTransform(); tr != nullptr; tr = tr->GetParent())
    {
        auto go = tr->GetGameObject();
        if (!go.IsValid()) continue;
        auto rb = go->GetComponent<RigidBodyComponent>();
        if (rb.IsValid()) last = rb;
    }

    if (last) return last->GetType() == RigidBodyComponent::Type::Dynamic;
    return true; // 못 찾으면 Dynamic(=Convex)으로 가정
}


bool MMMEngine::MeshColliderComponent::SetPhysicsActive(bool enable)
{
    if (enable) return TryBuildAndRegister();
    UnregisterFromPhysics(PhysicsUnregisterReason::Disable);
    return true;
}
