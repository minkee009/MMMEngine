#include "PhysxManager.h"
#include "ColliderComponent.h"
#include "RigidBodyComponent.h"
#include "Transform.h"
#include "GameObject.h"
#include "PhysxHelper.h"
#include "rttr/registration"

RTTR_REGISTRATION
{
    using namespace rttr;
    using namespace MMMEngine;

    registration::class_<ColliderComponent>("ColliderComponent")
        (rttr::metadata("INSPECTOR", "DONT_ADD_COMP"))
        .property("StaticFriction", &ColliderComponent::GetStaticFriction, &ColliderComponent::SetStaticFriction)
        .property("DynamicFriction", &ColliderComponent::GetDynamicFriction, &ColliderComponent::SetDynamicFriction)
        .property("Restitution", &ColliderComponent::GetRestitution, &ColliderComponent::SetRestitution)
        .property("Mode", &ColliderComponent::GetShapeMode, &ColliderComponent::SetShapeMode)
        .property("SetOverLayer", &ColliderComponent::GetOverrideLayer, &ColliderComponent::SetOverrideLayer)
            (rttr::metadata("INSPECTOR_CHAIN", "true=LayerType"))
        .property("LayerType", &ColliderComponent::GetLayer, &ColliderComponent::SetLayer)
            (rttr::metadata("RANGE", "0,15"));
    registration::enumeration<ColliderComponent::ShapeMode>("ShapeMode")
        (
            rttr::value("Simulation", ColliderComponent::ShapeMode::Simulation),
            rttr::value("Trigger", ColliderComponent::ShapeMode::Trigger),
            rttr::value("QueryOnly", ColliderComponent::ShapeMode::QueryOnly),
            rttr::value("Disabled", ColliderComponent::ShapeMode::Disabled)
            );
    //registration::class_<ColliderComponent::ShapeMode>("Mode")
    //    .property("Mode", &ColliderComponent::m_Mode);
}


void MMMEngine::ColliderComponent::EnsureMaterial()
{
	auto& physics = MMMEngine::PhysicX::Get().GetPhysics();
	if (!m_Material)
	{
		m_Material = physics.createMaterial(m_StaticFriction, m_DynamicFriction, m_Restitution);
		m_MaterialOwned = true;
	}
}

void MMMEngine::ColliderComponent::SetTriggerQueryEnabled(bool on)
{
    m_TriggerQueryEnabled = on;
    ApplySceneQueryFlag();
}

bool MMMEngine::ColliderComponent::SetPhysicsActive(bool enable)
{
    if (enable)
    {
        if (!m_Shape) return false;
        ApplyGeometryIfDirty();
        if (!m_IsRegistered) RegisterToPhysics();
        return true;
    }
    else
    {
        UnregisterFromPhysics(PhysicsUnregisterReason::Disable);
        return true;
    }
}

void MMMEngine::ColliderComponent::RegisterToPhysics()
{
    if (m_IsRegistered || !m_Shape) return;
    MMMEngine::PhysxManager::Get().NotifyColliderAdded(this);
    GetGameObject()->GetTransform()->onUpdateTransformTree
        .AddListener<ColliderComponent, &ColliderComponent::NoticeCompoundCollider>(this);
    m_IsRegistered = true;
}

void MMMEngine::ColliderComponent::UnregisterFromPhysics(PhysicsUnregisterReason reason)
{
    if (!m_IsRegistered && reason == PhysicsUnregisterReason::Disable)
        return;

    GetGameObject()->GetTransform()->onUpdateTransformTree
        .RemoveListener<ColliderComponent, &ColliderComponent::NoticeCompoundCollider>(this);

    //파괴와 떼는거 분리
    if (reason == PhysicsUnregisterReason::Disable)
    {
        MMMEngine::PhysxManager::Get().NotifyColliderDisabled(this); // detach만
    }
    else
    {
        MMMEngine::PhysxManager::Get().NotifyColliderRemoved(this);  // destroy 전용
    }

    m_IsRegistered = false;
}

//
MMMEngine::ColliderComponent::WorldBounds MMMEngine::ColliderComponent::GetWorldBounds(float inflation) const
{
    WorldBounds out{};
    if (!m_Shape) return out;

    auto* actor = m_Shape->getActor();
    if (!actor) return out;

    physx::PxBounds3 b = physx::PxShapeExt::getWorldBounds(*m_Shape, *actor, inflation);
    out.min = ToVec(b.minimum);
    out.max = ToVec(b.maximum);
    out.valid = true;
    return out;
}

float MMMEngine::ColliderComponent::GetApproxRadiusXZ(float inflation) const
{
    auto b = GetWorldBounds(inflation);
    if (!b.valid) return 0.0f;
    Vector3 e = b.Extents();
    return std::max(e.x, e.z);
}

MMMEngine::ColliderComponent::BoundingSphere MMMEngine::ColliderComponent::GetBoundingSphere(float inflation) const
{
    BoundingSphere s{};
    auto b = GetWorldBounds(inflation);
    if (!b.valid) return s;

    Vector3 e = b.Extents();
    s.center = b.Center();
    s.radius = e.Length();
    s.valid = true;
    return s;
}

void MMMEngine::ColliderComponent::ApplyMaterial()
{
	EnsureMaterial();
	if (!m_Material) return;
	m_Material->setStaticFriction(m_StaticFriction);
	m_Material->setDynamicFriction(m_DynamicFriction);
	m_Material->setRestitution(m_Restitution);
	if (m_Shape)
	{
		physx::PxMaterial* mats[1] = { m_Material };
		m_Shape->setMaterials(mats, 1);
	}
}

void MMMEngine::ColliderComponent::SetStaticFriction(float value)
{
	if (value < 0.0f) value = 0.0f;
	m_StaticFriction = value;
	ApplyMaterial();
}

void MMMEngine::ColliderComponent::SetDynamicFriction(float value)
{
	if (value < 0.0f) value = 0.0f;
	m_DynamicFriction = value;
	ApplyMaterial();
}

void MMMEngine::ColliderComponent::SetRestitution(float value)
{
	if (value < 0.0f) value = 0.0f;
	m_Restitution = value;
	ApplyMaterial();
}

void MMMEngine::ColliderComponent::ApplySceneQueryFlag()
{
    if (!m_Shape) return;

    bool query = m_SceneQueryEnabled;

    if (m_Mode == ShapeMode::Disabled) query = false;
    if (m_Mode == ShapeMode::QueryOnly) query = true;
    if (m_Mode == ShapeMode::Trigger) query = m_TriggerQueryEnabled;

    m_Shape->setFlag(physx::PxShapeFlag::eSCENE_QUERY_SHAPE, query);
}

//*** 레이어 규칙 변경했는데 즉시 반영이 안된다면 여기를 건드리기
void MMMEngine::ColliderComponent::ApplyFilterData()
{
    if (!m_Shape) return;
    m_Shape->setSimulationFilterData(m_SimFilter);
    m_Shape->setQueryFilterData(m_QueryFilter);
}

void MMMEngine::ColliderComponent::SetRigidOffsetPose(const physx::PxTransform& pose)
{
    m_RigidOffsetPose = pose;
    ApplyLocalPose();
}

void MMMEngine::ColliderComponent::SetCompoundCollider(ObjPtr<Transform> parent)
{
    NoticeCompoundCollider(parent);
}

void MMMEngine::ColliderComponent::ApplyLocalPose()
{
    if (!m_Shape) return;
    m_Shape->setLocalPose(m_RigidOffsetPose * m_LocalPose);
}

void MMMEngine::ColliderComponent::ApplyShapeModeFlags()
{
    if (!m_Shape) return;

    switch (m_Mode)
    {
    case ShapeMode::Simulation:
        m_Shape->setFlag(physx::PxShapeFlag::eSIMULATION_SHAPE, true);
        m_Shape->setFlag(physx::PxShapeFlag::eTRIGGER_SHAPE, false);
        break;

    case ShapeMode::Trigger:
        m_Shape->setFlag(physx::PxShapeFlag::eSIMULATION_SHAPE, false);
        m_Shape->setFlag(physx::PxShapeFlag::eTRIGGER_SHAPE, true);
        break;

    case ShapeMode::QueryOnly:
        m_Shape->setFlag(physx::PxShapeFlag::eSIMULATION_SHAPE, false);
        m_Shape->setFlag(physx::PxShapeFlag::eTRIGGER_SHAPE, false);
        break;

    case ShapeMode::Disabled:
        m_Shape->setFlag(physx::PxShapeFlag::eSIMULATION_SHAPE, false);
        m_Shape->setFlag(physx::PxShapeFlag::eTRIGGER_SHAPE, false);
        break;
    }
}

void MMMEngine::ColliderComponent::SetOverrideLayer(bool enable)
{
    m_OverrideLayer = enable;
    if (m_Shape)
    {
        MarkFilterDirty();
    }
}

/// <summary>
/// 이번 프로젝트에서 
/// </summary>
/// <param name="layer"></param>
void MMMEngine::ColliderComponent::SetLayer(uint32_t layer)
{
    if (m_LayerOverride == layer) return;
    m_LayerOverride = layer;
    if (m_Shape)
    {
        MarkFilterDirty();
    }
}

uint32_t MMMEngine::ColliderComponent::GetEffectiveLayer()
{
    if (m_OverrideLayer) return m_LayerOverride;
    else
    {
        if(GetGameObject().IsValid())
		{
			const auto Obj_layer = GetGameObject()->GetLayer();
			return Obj_layer;
		}
        else
        {
            return 0;
        }
    }  
}

void MMMEngine::ColliderComponent::MarkGeometryDirty()
{
    m_geometryDirty = true;
    PhysxManager::Get().NotifyColliderChanged(this);
}

bool MMMEngine::ColliderComponent::ApplyGeometryIfDirty()
{
    if (!m_geometryDirty)
        return false;

    if (!m_Shape)
        return false;

    const bool ok = UpdateShapeGeometry();

#ifdef _DEBUG
    if (!ok)
        OutputDebugStringA("[Collider] UpdateShapeGeometry failed.\n");
#endif

    if (ok)
    {
        m_geometryDirty = false;
        ApplyAll(); // geometry 바뀐 뒤 flags/filter/pose 재적용
    }

    return ok;
}


//Trigger가 Scene Query에서 기본적으로 빠지는 정책인 상태 아니면 여기 수정
void MMMEngine::ColliderComponent::SetShapeMode(ShapeMode mode)
{
    m_Mode = mode;

    switch (m_Mode)
    {
    case ShapeMode::Simulation: m_SceneQueryEnabled = true;  break;
    case ShapeMode::Trigger:    m_SceneQueryEnabled = false; break;
    case ShapeMode::QueryOnly:  m_SceneQueryEnabled = true;  break;
    case ShapeMode::Disabled:   m_SceneQueryEnabled = false; break;
    }

    ApplyAll();
}

//void MMMEngine::ColliderComponent::SetLocalPose(const physx::PxTransform& t)
//{
//    m_LocalPose = t; ApplyAll();
//}

void MMMEngine::ColliderComponent::SetLocalCenter(Vector3 pos)
{
    m_LocalCenter = pos;
    auto pxPos = ToPxVec(m_LocalCenter);
    m_LocalPose.p = pxPos;
    ApplyLocalPose();
}

void MMMEngine::ColliderComponent::SetLocalRotation(Quaternion quater)
{
    m_LocalQuater = quater;
    auto pxQuat = ToPxQuat(m_LocalQuater);
    m_LocalPose.q = pxQuat;
    ApplyLocalPose();
}

Vector3 MMMEngine::ColliderComponent::GetLocalCenter()
{
    return m_LocalCenter;
}

Quaternion MMMEngine::ColliderComponent::GetLocalQuater()
{
    return m_LocalQuater;
}


void MMMEngine::ColliderComponent::SetSceneQueryEnabled(bool on)
{
    m_SceneQueryEnabled = on; ApplyAll();
}

void MMMEngine::ColliderComponent::SetFilterData(const physx::PxFilterData& sim, const physx::PxFilterData& query)
{
    m_SimFilter = sim; m_QueryFilter = query; ApplyAll();
}

void MMMEngine::ColliderComponent::MarkFilterDirty()
{
    m_filterDirty = true;

    // 
    PhysxManager::Get().NotifyColliderChanged(this);
}

physx::PxTransform MMMEngine::ColliderComponent::GetWorldPosPx() const
{
    if (!m_Shape) return physx::PxTransform(physx::PxIdentity);

    physx::PxRigidActor* actor = m_Shape->getActor();
    if (!actor) return physx::PxTransform(physx::PxIdentity);

    // 
    return actor->getGlobalPose() * m_Shape->getLocalPose();
}


void MMMEngine::ColliderComponent::SetChildValue(ObjPtr<Transform> T)
{
    if(T != nullptr)  Child_value = true;
}

bool MMMEngine::ColliderComponent::GetChildValue()
{
    return Child_value;
}

void MMMEngine::ColliderComponent::NoticeCompoundCollider(ObjPtr<Transform> preParent)
{
    ObjPtr<GameObject> NextParent_obj{};
    if (preParent.IsValid())
    {
        NextParent_obj = preParent->GetGameObject();
    }
    ObjPtr<GameObject> CurParent_obj{};
    auto CurParent = GetTransform()->GetParent();
    if (CurParent.IsValid())
    {
        CurParent_obj = CurParent->GetGameObject();
    }
    auto Self_obj = GetGameObject();

    MMMEngine::PhysxManager::Get().NotifyCompoundColliderAdded(NextParent_obj, CurParent_obj, Self_obj);
}

void MMMEngine::ColliderComponent::SetLocalShape()
{
    if (!m_Shape) return;
    auto* actor = m_Shape->getActor();
    if (!actor) return;

    Vector3 goWorldPos = GetTransform()->GetWorldPosition();
    Quaternion goWorldRot = GetTransform()->GetWorldRotation();
    physx::PxTransform goWorldPx = ToPxTrans(goWorldPos, goWorldRot);

    physx::PxTransform actorWorld = actor->getGlobalPose();
    physx::PxTransform rigidOffset = actorWorld.getInverse() * goWorldPx;

    SetRigidOffsetPose(rigidOffset);
}

void MMMEngine::ColliderComponent::SetShape(physx::PxShape* shape, bool owned)
{
    if (m_Shape)
    {
        if (physx::PxRigidActor* actor = m_Shape->getActor())
            actor->detachShape(*m_Shape);

        if (m_Owned)
            m_Shape->release();
    }
    //if (m_Shape && m_Owned) m_Shape->release();

    m_Shape = shape;
    m_Owned = owned;

    if (m_Shape)
    {
        m_Shape->userData = this; // PhysScene 이벤트 매핑용(핵심)
        ApplyAll();
    }
}

void MMMEngine::ColliderComponent::ApplyAll()
{
    ApplyShapeModeFlags();
    ApplySceneQueryFlag();
    ApplyFilterData();
    ApplyLocalPose();
}


void MMMEngine::ColliderComponent::Initialize()
{
	//
    if (auto& obj = GetGameObject(); obj.IsValid())
    {
        obj->onActiveInHierarchyChanged.AddListener<ColliderComponent, &ColliderComponent::OnOwnerActiveInHierarchyChanged>(this);
    }


	auto& physics = MMMEngine::PhysicX::Get().GetPhysics();
	EnsureMaterial();
	physx::PxMaterial* mat = m_Material ? m_Material : MMMEngine::PhysicX::Get().GetDefaultMaterial();

	if (mat)
	{
        if (!BuildShape(&physics, mat) || !m_Shape)
        {
            std::cout << u8"Shape 생성 실패 , BuildShape를 확인" << std::endl;
            return;
        }
	}

    if (GetGameObject()->IsActiveInHierarchy())
        RegisterToPhysics();
}

void MMMEngine::ColliderComponent::UnInitialize()
{
    if (auto& obj = GetGameObject(); obj.IsValid())
    {
        obj->onActiveInHierarchyChanged.RemoveListener<ColliderComponent, &ColliderComponent::OnOwnerActiveInHierarchyChanged>(this);
    }

    if(GetGameObject().IsValid())
    {
        UnregisterFromPhysics(PhysicsUnregisterReason::Destroy);
    }

    //PhysxManager::Get().NotifyColliderRemoved(this);
    if (m_Shape)
    {
        if (auto* actor = m_Shape->getActor())
            actor->detachShape(*m_Shape);

        if (m_Owned)
            m_Shape->release();

        m_Shape = nullptr;
        m_Owned = false;
    }

    if (m_Material && m_MaterialOwned)
    {
        m_Material->release();
    }
    m_Material = nullptr;
    m_MaterialOwned = false;
}

void MMMEngine::ColliderComponent::DetachShapeFromActor()
{
    if (m_Shape)
    {
        if (auto* actor = m_Shape->getActor())
            actor->detachShape(*m_Shape);
    }
}

void MMMEngine::ColliderComponent::AttachShapeFromActor(physx::PxRigidActor* Actor)
{

    if (!Actor || !m_Shape) return;
    
    Actor->attachShape(*m_Shape);

    return;
}


void MMMEngine::ColliderComponent::OnOwnerActiveInHierarchyChanged()
{
    if (!GetGameObject().IsValid()) return;

    if (GetGameObject()->IsActiveInHierarchy())
        SetPhysicsActive(true);   // RegisterToPhysics 내부 호출
    else
        SetPhysicsActive(false);  // UnregisterFromPhysics 내부 호출
}
