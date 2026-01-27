#include "PhysxManager.h"
#include "SceneManager.h"
#include "GameObject.h"
#include "BehaviourManager.h"

DEFINE_SINGLETON(MMMEngine::PhysxManager)

void MMMEngine::PhysxManager::BindScene(MMMEngine::Scene* scene)
{
    if (m_Scene == scene) return;

    UnbindScene();

    m_Scene = scene;
    if (!m_Scene) return;

    PhysSceneDesc desc{};
    
    for (uint32_t i = 0; i <= 4; ++i)
    {
        for (uint32_t j = 0; j <= 4; ++j)
        {
            m_CollisionMatrix.SetCanCollide(i, j, true);
        }
    }

    if (m_PhysScene.Create(desc))
    {
        m_IsInitialized = true;
        m_FilterDirty = true;
    }
    else
    {
        std::cout << "Scene create failed" << std::endl;
        m_Scene = nullptr;
    }
}

void MMMEngine::PhysxManager::StepFixed(float dt)
{
    if (!m_IsInitialized) return;
    if (dt <= 0.f) return;
	FlushCommands_PreStep();     

	ApplyFilterConfigIfDirty();  
    FlushDirtyColliders_PreStep(); 

    m_PhysScene.PushRigidsToPhysics(); 
	m_PhysScene.Step(dt);       
    m_PhysScene.PullRigidsFromPhysics();  
    m_PhysScene.DrainEvents();            

    DispatchPhysicsEvents();

	FlushCommands_PostStep();  
}

void MMMEngine::PhysxManager::NotifyRigidAdded(RigidBodyComponent* rb)
{
    if (!rb) return;
    RequestRegisterRigid(rb);
    
}

void MMMEngine::PhysxManager::NotifyRigidRemoved(RigidBodyComponent* rb)
{
    if (!rb) return;
    RequestUnregisterRigid(rb);
}

void MMMEngine::PhysxManager::NotifyColliderAdded(ColliderComponent* col)
{
    if (!col) return;

    auto go = col->GetGameObject();
    if (!go.IsValid()) return;

    auto rbPtr = go->GetComponent<RigidBodyComponent>();
    RigidBodyComponent* rb = rbPtr.IsValid() ? (RigidBodyComponent*)rbPtr.GetRaw()
        : GetOrCreateRigid(go);

    if (!rb) return;

    RequestRegisterRigid(rb);
    RequestAttachCollider(rb, col);
}

void MMMEngine::PhysxManager::NotifyColliderRemoved(ColliderComponent* col)
{
    if (!col) return;

    auto go = col->GetGameObject();
    if (!go.IsValid()) return;

    auto rbPtr = go->GetComponent<RigidBodyComponent>();
    if (!rbPtr.IsValid()) return; 
    auto* rb = (RigidBodyComponent*)rbPtr.GetRaw();

    RequestDetachCollider(rb, col);
}

void MMMEngine::PhysxManager::NotifyColliderChanged(ColliderComponent* col)
{
    if (!col) return;

    if (col->IsGeometryDirty())
        m_DirtyColliders.insert(col);

    if (col->IsFilterDirty())
        m_FilterDirtyColliders.insert(col);
}


void MMMEngine::PhysxManager::RequestRegisterRigid(MMMEngine::RigidBodyComponent* rb)
{
    if (!rb) return;
    if (m_PendingUnreg.find(rb) != m_PendingUnreg.end()) return;

    for (auto it = m_Commands.begin(); it != m_Commands.end(); )
    {
        if (it->type == CmdType::RegRigid && it->new_rb == rb)
            it = m_Commands.erase(it);
        else
            ++it;
    }

    m_Commands.push_back({ CmdType::RegRigid, rb, nullptr });
}

void MMMEngine::PhysxManager::RequestUnregisterRigid(MMMEngine::RigidBodyComponent* rb)
{
    if (!rb) return;

    if (m_PendingUnreg.find(rb) != m_PendingUnreg.end()) return;

	for (auto it = m_Commands.begin(); it != m_Commands.end(); )
	{
		if (it->new_rb == rb)
			it = m_Commands.erase(it);
		else
			++it;
	}
    m_PendingUnreg.insert(rb);
    m_Commands.push_back({ CmdType::UnregRigid, rb, nullptr });
}

void MMMEngine::PhysxManager::RequestAttachCollider(MMMEngine::RigidBodyComponent* rb, MMMEngine::ColliderComponent* col)
{
	if (!rb || !col) return;
    if (m_PendingUnreg.find(rb) != m_PendingUnreg.end())
    {
        return;
    }

    for (auto it = m_Commands.begin(); it != m_Commands.end(); )
    {
        if (it->type == CmdType::DetachCol && it->new_rb == rb && it->col == col)
            it = m_Commands.erase(it);
        else
            ++it;
    }
	m_Commands.push_back({ CmdType::AttachCol, rb, col });
}

void MMMEngine::PhysxManager::RequestDetachCollider(MMMEngine::RigidBodyComponent* rb, MMMEngine::ColliderComponent* col)
{
    if (!rb || !col) return;

    if (m_PendingUnreg.find(rb) != m_PendingUnreg.end()) return;
    for (auto it = m_Commands.begin(); it != m_Commands.end(); )
    {
        if (it->type == CmdType::AttachCol && it->new_rb == rb && it->col == col)
            it = m_Commands.erase(it);
        else
            ++it;
    }

    m_Commands.push_back({ CmdType::DetachCol, rb, col });
}

void MMMEngine::PhysxManager::RequestRebuildCollider(MMMEngine::RigidBodyComponent* rb, MMMEngine::ColliderComponent* col)
{
	if (!col) return;

    for (auto it = m_Commands.begin(); it != m_Commands.end(); )
    {
        if (it->type == CmdType::RebuildCol && it->col == col)
            it = m_Commands.erase(it);
        else
            ++it;
    }
    m_Commands.push_back({ CmdType::RebuildCol, rb, col });
}

void MMMEngine::PhysxManager::RequestReapplyFilters()
{
    m_FilterDirty = true;
}

void MMMEngine::PhysxManager::RequestChangeRigidType(MMMEngine::RigidBodyComponent* rb)
{
    if (!rb) return;

    // Unregister 예정이면 타입 바꿀 의미가 없음 (어차피 사라짐)
    if (m_PendingUnreg.find(rb) != m_PendingUnreg.end())
        return;

    //같은 rb에 대한 이전 ChangeRigidType 요청이 있으면 제거 (마지막 요청만 남김)
    for (auto it = m_Commands.begin(); it != m_Commands.end(); )
    {
        if (it->type == CmdType::ChangeRigid && it->new_rb == rb)
            it = m_Commands.erase(it);
        else
            ++it;
    }

    // 정책: 타입 변경은 "actor 재생성"이라, 기존 Attach/Detach이 뒤섞이면 위험
    //    - 가장 안전한 정책은: 타입 변경 요청 시점에 rb 관련 Attach/Detach을 정리하거나
    //    - 혹은 Flush 순서를 ChangeRigidType -> Attach/Detach로 강제하는 것
    //
    // 여기서는 "Flush 순서 강제"로 가는 게 보통 더 낫다.
    // 따라서 여기서는 지우지 않고, FlushCommands_PreStep에서 ChangeRigidType을 먼저 처리하게 만든다.

    m_Commands.push_back({ CmdType::ChangeRigid, rb, nullptr });
}

MMMEngine::RigidBodyComponent* MMMEngine::PhysxManager::GetOrCreateRigid(ObjPtr<GameObject> go)
{
    if (!go.IsValid()) return nullptr;

    go->AddComponent<RigidBodyComponent>();
    auto auto_rb = go->GetComponent<RigidBodyComponent>();
    return auto_rb.IsValid() ? static_cast<RigidBodyComponent*>(auto_rb.GetRaw()) : nullptr;
}

bool MMMEngine::PhysxManager::HasAnyCollider(ObjPtr<GameObject> go) const
{
    if (!go.IsValid()) return false;

    return (go->GetComponent<ColliderComponent>().IsValid());
}


void MMMEngine::PhysxManager::SetSceneGravity(float x, float y, float z)
{
    m_PhysScene.SetGravity(x, y, z);
}

void MMMEngine::PhysxManager::SetLayerCollision(uint32_t layerA, uint32_t layerB, bool canCollide)
{
    m_CollisionMatrix.SetCanCollide(layerA, layerB, canCollide);
    m_FilterDirty = true;
}

void MMMEngine::PhysxManager::FlushCommands_PreStep()
{
    for (auto it = m_Commands.begin(); it != m_Commands.end(); )
    {
        if (it->type == CmdType::ChangeRigid)
        {
            m_PhysScene.ChangeRigidType(it->new_rb, m_CollisionMatrix);
            it = m_Commands.erase(it);
        }
        else ++it;
    }


    for (auto it = m_Commands.begin(); it != m_Commands.end(); )
    {
        switch (it->type)
        {
        case CmdType::RegRigid:
            m_PhysScene.RegisterRigid(it->new_rb);
            it = m_Commands.erase(it);
            break;

        case CmdType::AttachCol:
            m_PhysScene.AttachCollider(it->new_rb, it->col, m_CollisionMatrix);
            it = m_Commands.erase(it);
            break;

        case CmdType::RebuildCol:
             m_PhysScene.RebuildCollider(it->col, m_CollisionMatrix);
            it = m_Commands.erase(it);
            break;
        default:
            ++it;
            break;
        }
    }
}


void MMMEngine::PhysxManager::FlushCommands_PostStep()
{
    //Detach
    for (auto it = m_Commands.begin(); it != m_Commands.end(); )
    {
        if (it->type == CmdType::DetachCol)
        {
            auto* rb = it->new_rb;
            auto* col = it->col;

            if (!rb || m_PendingUnreg.find(rb) != m_PendingUnreg.end())
            {
                it = m_Commands.erase(it);
                continue;
            }

            if (rb->GetPxActor() == nullptr)
            {
                it = m_Commands.erase(it);
                continue;
            }

            m_PhysScene.DetachCollider(rb, col);
            it = m_Commands.erase(it);
        }
        else
        {
            ++it;
        }
    }

    // Unregister
    for (auto it = m_Commands.begin(); it != m_Commands.end(); )
    {
        if (it->type == CmdType::UnregRigid)
        {
            auto* rb = it->new_rb;

            if (rb)
            {
                m_PhysScene.UnregisterRigid(rb); 
                m_PendingUnreg.erase(rb);
            }

            it = m_Commands.erase(it);
        }
        else
        {
            ++it;
        }
    }

}

void MMMEngine::PhysxManager::ApplyFilterConfigIfDirty()
{
    if (!m_FilterDirty) return;
    if (!m_Scene) return;

    m_PhysScene.ReapplyFilters(m_CollisionMatrix);

    m_FilterDirty = false;
}

void MMMEngine::PhysxManager::FlushDirtyColliders_PreStep()
{
    if (m_DirtyColliders.empty()) return;

    for (auto* col : m_DirtyColliders)
    {
        if (!col) continue;
        m_PhysScene.UpdateColliderGeometry(col);
    }
    m_DirtyColliders.clear();
}

void MMMEngine::PhysxManager::FlushDirtyColliderFilters_PreStep()
{
    if (m_FilterDirtyColliders.empty()) return;

    for (auto* col : m_FilterDirtyColliders)
    {
        if (!col) continue;
        if (!col->GetPxShape()) { col->ClearFilterDirty(); continue; } 

        const uint32_t layer = col->GetEffectiveLayer();
        col->SetFilterData(m_CollisionMatrix.MakeSimFilter(layer),
            m_CollisionMatrix.MakeQueryFilter(layer));

        m_PhysScene.ResetFilteringFor(col);

        col->ClearFilterDirty();
    }
    m_FilterDirtyColliders.clear();
}



void MMMEngine::PhysxManager::EraseCommandsForRigid(MMMEngine::RigidBodyComponent* rb)
{
    if (!rb) return;
    for (auto it = m_Commands.begin(); it != m_Commands.end(); )
    {
        if (it->new_rb == rb) it = m_Commands.erase(it);
        else ++it;
    }
}

void MMMEngine::PhysxManager::EraseAttachDetachForRigid(MMMEngine::RigidBodyComponent* rb)
{
    if (!rb) return;

    for (auto it = m_Commands.begin(); it != m_Commands.end(); )
    {
        if ((it->new_rb == rb) &&
            (it->type == CmdType::AttachCol || it->type == CmdType::DetachCol))
            it = m_Commands.erase(it);
        else
            ++it;
    }
}

void MMMEngine::PhysxManager::EraseCommandsForCollider(MMMEngine::ColliderComponent* col)
{
    if (!col) return;

    for (auto it = m_Commands.begin(); it != m_Commands.end(); )
    {
        if (it->col == col &&
            (it->type == CmdType::AttachCol || it->type == CmdType::DetachCol || it->type == CmdType::RebuildCol))
            it = m_Commands.erase(it);
        else
            ++it;
    }
}

void MMMEngine::PhysxManager::NotifyRigidTypeChanged(RigidBodyComponent* rb)
{
    if (!rb) return;
    RequestChangeRigidType(rb); 
}

void MMMEngine::PhysxManager::UnbindScene()
{
    m_Commands.clear();
    m_DirtyColliders.clear();
    m_PendingUnreg.clear();
    m_FilterDirty = false;

    if (m_IsInitialized)
    {
        m_PhysScene.Destroy();
        m_IsInitialized = false;
    }

    m_Scene = nullptr;

    m_PhysScene.Destroy();
}

void MMMEngine::PhysxManager::DispatchPhysicsEvents()
{
    const auto& contacts = m_PhysScene.GetFrameContacts();
    for (const auto& e : contacts)
    {
        auto* rbA = static_cast<RigidBodyComponent*>(e.a ? e.a->userData : nullptr);
        auto* rbB = static_cast<RigidBodyComponent*>(e.b ? e.b->userData : nullptr);
        auto* colA = static_cast<ColliderComponent*>(e.aShape ? e.aShape->userData : nullptr);
        auto* colB = static_cast<ColliderComponent*>(e.bShape ? e.bShape->userData : nullptr);

        if (!rbA || !rbB || !colA || !colB) continue;

        auto goA = rbA->GetGameObject();
        auto goB = rbB->GetGameObject();
        if (!goA.IsValid() || !goB.IsValid()) continue;

        const bool enter = (e.events & physx::PxPairFlag::eNOTIFY_TOUCH_FOUND) != 0;
        const bool stay = (e.events & physx::PxPairFlag::eNOTIFY_TOUCH_PERSISTS) != 0;
        const bool exit = (e.events & physx::PxPairFlag::eNOTIFY_TOUCH_LOST) != 0;


        if (enter) Callback_Que.push_back({ goA, goB, P_EvenType::C_enter });
        if (stay)  Callback_Que.push_back({ goA, goB, P_EvenType::C_stay });
        if (exit)  Callback_Que.push_back({ goA, goB, P_EvenType::C_out });
    }

    // 2) Trigger
    const auto& triggers = m_PhysScene.GetFrameTriggers();
    for (const auto& t : triggers)
    {
        auto* triggerCol = static_cast<ColliderComponent*>(t.triggerShape ? t.triggerShape->userData : nullptr);
        auto* otherCol = static_cast<ColliderComponent*>(t.otherShape ? t.otherShape->userData : nullptr);
        if (!triggerCol || !otherCol) continue;

        auto goT = triggerCol->GetGameObject();
        auto goO = otherCol->GetGameObject();
        if (!goT.IsValid() || !goO.IsValid()) continue;

        if (t.isEnter)
        {
            Callback_Que.push_back({ goT, goO, P_EvenType::T_enter });

        }
        else
        {
            Callback_Que.push_back({ goT, goO, P_EvenType::T_out });
        }
    }
}


void MMMEngine::PhysxManager::Shutdown()
{
    UnbindScene();
}
