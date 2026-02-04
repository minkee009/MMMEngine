#include "ObjectManager.h"
#include "ObjectSerializer.h"
#include "Transform.h"
#include "SceneManager.h"
#include "SerializableEvent.h"

DEFINE_SINGLETON(MMMEngine::ObjectManager)

namespace MMMEngine {
    static thread_local bool s_isCreatingObject = false;
    static thread_local bool s_isDestroyingObject = false;

    ObjectManager::CreationScope::CreationScope() {
        s_isCreatingObject = true;
    }
    ObjectManager::CreationScope::~CreationScope() {
        s_isCreatingObject = false;
    }

    ObjectManager::DestroyScope::DestroyScope() {
        s_isDestroyingObject = true;
    }
    ObjectManager::DestroyScope::~DestroyScope() {
        s_isDestroyingObject = false;
    }
}

void MMMEngine::ObjectManager::UpdateInternalTimer(float deltaTime)
{
    size_t i = 0;
    while (i < m_delayedDestroy.size())
    {
        const uint32_t id = m_delayedDestroy[i];
        if (id >= m_objectPtrInfos.size())
        {
            m_delayedDestroy[i] = m_delayedDestroy.back();
            m_delayedDestroy.pop_back();
            continue;
        }

        auto& info = m_objectPtrInfos[id];
        if (!info.raw || info.raw->IsDestroyed() || info.destroyRemainTime < 0.0f)
        {
            info.destroyScheduled = false;
            info.destroyRemainTime = -1.0f;

            m_delayedDestroy[i] = m_delayedDestroy.back();
            m_delayedDestroy.pop_back();
            continue;
        }
        info.destroyRemainTime -= deltaTime;

        if (info.destroyRemainTime <= 0.0f)
        {
            m_pendingDestroy.push_back(id);

            // �ı� ������ destroyed ���·� ��ȯ
            UnregisterObjectMUID(info.raw);
            info.raw->MarkDestroy();

            // delayed���� ����
            info.destroyScheduled = false;
            info.destroyRemainTime = -1.0f;

            m_delayedDestroy[i] = m_delayedDestroy.back();
            m_delayedDestroy.pop_back();
            continue;
        }

        ++i;
    }
}

void MMMEngine::ObjectManager::ProcessPendingDestroy()
{
    DestroyScope scope;

    for (uint32_t ptrID : m_pendingDestroy)
    {
        if (ptrID >= m_objectPtrInfos.size())
            continue;

        Object* obj = m_objectPtrInfos[ptrID].raw;
        if (!obj)
            continue;

        delete obj;
        m_objectPtrInfos[ptrID].raw = nullptr;
        m_freePtrIDs.push(ptrID);
    }

    m_pendingDestroy.clear();
}

bool MMMEngine::ObjectManager::IsCreatingObject()
{
    return s_isCreatingObject;
}

bool MMMEngine::ObjectManager::IsDestroyingObject()
{
    return s_isDestroyingObject;
}

bool MMMEngine::ObjectManager::IsValidPtr(uint32_t ptrID, uint32_t generation, const void* ptr) const
{
    if (ptrID >= m_objectPtrInfos.size())
        return false;

    Object* stored = m_objectPtrInfos[ptrID].raw;

    if (static_cast<const void*>(stored) != ptr)
        return false;

    if (m_objectPtrInfos[ptrID].ptrGenerations != generation)
        return false;

    return true;
}

void MMMEngine::ObjectManager::Destroy(const ObjPtrBase& objPtr, float delayTime)
{
    if (!objPtr.IsValid() || static_cast<Object*>(objPtr.GetRaw())->IsDestroyed())
        return;

    auto id = objPtr.GetPtrID();
    auto& info = m_objectPtrInfos[id];
    if (delayTime <= 0.0f)
    {
        m_pendingDestroy.push_back(id);
        UnregisterObjectMUID(info.raw);
        info.raw->MarkDestroy();
        return;
    }

    // ���� �ı� ���� (�Ǵ� �մ���)
    if (info.destroyRemainTime < 0.0f)
    {
        info.destroyRemainTime = delayTime;

        if (!info.destroyScheduled)
        {
            info.destroyScheduled = true;
            m_delayedDestroy.push_back(id);
        }
    }
    else
    {
        // �� ���� �ð��� �ݿ�
        if (delayTime < info.destroyRemainTime)
            info.destroyRemainTime = delayTime;
    }
}

void MMMEngine::ObjectManager::StartUp()
{
    SerializableEvent::SetResolver([](const Utility::MUID& muid) { return ObjectManager::Get().GetObjectByMUID(muid); });
    SerializableEventT<float>::SetResolver([](const Utility::MUID& muid) { return ObjectManager::Get().GetObjectByMUID(muid); });
    SerializableEventT<bool>::SetResolver([](const Utility::MUID& muid) { return ObjectManager::Get().GetObjectByMUID(muid); });
    SerializableEventT<int>::SetResolver([](const Utility::MUID& muid) { return ObjectManager::Get().GetObjectByMUID(muid); });
    SerializableEventT<std::string>::SetResolver([](const Utility::MUID& muid) { return ObjectManager::Get().GetObjectByMUID(muid); });
}

void MMMEngine::ObjectManager::ShutDown()
{
    // ��� ��ü ����
    DestroyScope scope;

    // �ı� ���� ��ȿȭ
    m_pendingDestroy.clear();
    m_delayedDestroy.clear();

    for (ObjectPtrInfo& info : m_objectPtrInfos)
    {
        if (info.raw)
        {
            delete info.raw;
            info.raw = nullptr;
            info.ptrGenerations = 0;
            info.destroyRemainTime = -1.0f;
            info.destroyScheduled = false;
        }
    }

    m_objectPtrInfos.clear();
    m_objectPtrInfos.shrink_to_fit();

    m_muidTable.clear();


    // free id ���� ����
    while (!m_freePtrIDs.empty())
        m_freePtrIDs.pop();
}

void MMMEngine::ObjectManager::RegisterObjectMUID(Object* obj)
{
    if (!obj)
        return;
    if (obj->m_ptrID == UINT32_MAX)
        return;

    const Utility::MUID& muid = obj->m_muid;
    if (muid.IsEmpty())
        return;

    m_muidTable[muid] = GetPtrFast<Object>(obj, obj->m_ptrID, obj->m_ptrGen);
}

void MMMEngine::ObjectManager::UnregisterObjectMUID(Object* obj)
{
    if (!obj)
        return;

    const Utility::MUID& muid = obj->m_muid;
    if (muid.IsEmpty())
        return;

    auto it = m_muidTable.find(muid);
    if (it == m_muidTable.end())
        return;

    Object* raw = static_cast<Object*>(it->second.GetRaw());
    if (raw == obj)
        m_muidTable.erase(it);
}

MMMEngine::ObjPtr<MMMEngine::Object> MMMEngine::ObjectManager::GetObjectByMUID(const Utility::MUID& muid) const
{
    if (muid.IsEmpty())
        return ObjPtr<Object>();

    auto it = m_muidTable.find(muid);
    if (it == m_muidTable.end())
    {
        for (uint32_t i = 0; i < m_objectPtrInfos.size(); ++i)
        {
            const auto& info = m_objectPtrInfos[i];
            if (!info.raw || info.raw->IsDestroyed())
                continue;
            if (!IsValidPtr(i, info.ptrGenerations, info.raw))
                continue;
            if (info.raw->m_muid == muid)
                return ObjPtr<Object>(info.raw, i, info.ptrGenerations);
        }
        return ObjPtr<Object>();
    }

    return it->second;
}

MMMEngine::ObjPtr<MMMEngine::Object> MMMEngine::ObjectManager::GetObjectByMUID(const std::string& muidStr) const
{
    auto parsed = Utility::MUID::Parse(muidStr);
    if (!parsed.has_value())
        return ObjPtr<Object>();

    return GetObjectByMUID(parsed.value());
}

void MMMEngine::ObjectManager::DontDestroyOnLoad(const ObjPtrBase& objPtr)
{
    // GameObject인 경우 그 자체를 씬에게 넘기기
    if (auto go = ObjectManager::Get().GetPtr<Object>(objPtr.GetPtrID(), objPtr.GetPtrGeneration()).Cast<GameObject>())
    {
        // 이미 파괴되었거나 이미 DontDestroyOnLoad 씬에 있으면 처리하지 않음
        if (go->IsDestroyed() || go->GetScene().id_DDOL)
            return;

        // 부모가 있는 경우 부모를 끊기
        go->GetTransform()->SetParent(nullptr);

        std::vector<ObjPtr<GameObject>> gameObjectsToProcess;
        gameObjectsToProcess.push_back(go);

        // BFS (너비 우선 탐색) 방식으로 계층 구조를 순회하여 스택 오버플로우 방지
        while (!gameObjectsToProcess.empty())
        {
            ObjPtr<GameObject> currentGo = gameObjectsToProcess.back();
            gameObjectsToProcess.pop_back();

            // 이미 처리했거나 파괴되었거나 DontDestroyOnLoad 씬에 있으면 건너뜀
            if (currentGo->IsDestroyed() || currentGo->GetScene().id_DDOL)
                continue;

            // 자신을 현재 씬에서 해제하고 DontDestroyOnLoad 씬에 등록
            if (auto sceneRaw = SceneManager::Get().GetSceneRaw(currentGo->GetScene())) // 씬이 유효한지 확인
            {
                sceneRaw->UnRegisterGameObject(currentGo);
            }
            SceneManager::Get().RegisterGameObjectToDDOL(currentGo);

            // 자식들을 큐에 추가하여 다음 반복에서 처리
            for (size_t i = 0; i < currentGo->GetTransform()->GetChildCount(); ++i)
            {
                if (auto childGo = currentGo->GetTransform()->GetChild(i)->GetGameObject())
                {
                    gameObjectsToProcess.push_back(childGo);
                }
            }
        }
    }
}

MMMEngine::ObjPtr<MMMEngine::GameObject> MMMEngine::ObjectManager::Instantiate(const ObjPtr<GameObject>& original)
{
    return ObjectSerializer::Get().Instantiate(original);
}

MMMEngine::ObjPtr<MMMEngine::Component> MMMEngine::ObjectManager::Instantiate(const ObjPtr<Component>& original)
{
    return ObjectSerializer::Get().Instantiate(original);
}

MMMEngine::ObjPtr<MMMEngine::GameObject> MMMEngine::ObjectManager::Instantiate(const ResPtr<Prefab>& prefab)
{
    return ObjectSerializer::Get().Instantiate(prefab);
}

bool MMMEngine::ObjectManager::CreatePrefabFromGameObject(const ObjPtr<GameObject>& root,
    const std::filesystem::path& directory)
{
    return ObjectSerializer::Get().CreatePrefabFromGameObject(root, directory);
}

void MMMEngine::ObjectManager::UpdateObjectMUID(Object* obj, const Utility::MUID& oldMuid, const Utility::MUID& newMuid)
{
    if (!obj)
        return;
    if (obj->m_ptrID == UINT32_MAX)
        return;

    if (oldMuid == newMuid)
        return;

    if (!oldMuid.IsEmpty())
    {
        auto it = m_muidTable.find(oldMuid);
        if (it != m_muidTable.end())
        {
            Object* raw = static_cast<Object*>(it->second.GetRaw());
            if (raw == obj)
                m_muidTable.erase(it);
        }
    }

    if (!newMuid.IsEmpty())
    {
        m_muidTable[newMuid] = GetPtrFast<Object>(obj, obj->m_ptrID, obj->m_ptrGen);
    }
}

MMMEngine::ObjectManager::~ObjectManager()
{
    ShutDown();
}
