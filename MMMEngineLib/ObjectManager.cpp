#include "ObjectManager.h"

void MMMEngine::ObjectManager::Update(float deltaTime)
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

            // 파괴 직전에 destroyed 상태로 전환
            info.raw->MarkDestroy();

            // delayed에서 제거
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

bool MMMEngine::ObjectManager::IsCreatingObject() const
{
    return m_isCreatingObject;
}

bool MMMEngine::ObjectManager::IsDestroyingObject() const
{
    return m_isDestroyingObject;
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

    //if (stored->IsDestroyed())
    //    return false;

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
        info.raw->MarkDestroy();
        return;
    }

    // 지연 파괴 예약 (또는 앞당기기)
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
        // 더 빠른 시간만 반영
        if (delayTime < info.destroyRemainTime)
            info.destroyRemainTime = delayTime;
    }
}

void MMMEngine::ObjectManager::StartUp()
{

}

void MMMEngine::ObjectManager::ShutDown()
{
    // 모든 객체 정리
    DestroyScope scope;

    // 파괴 예약 무효화
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

    // free id 스택 비우기
    while (!m_freePtrIDs.empty())
        m_freePtrIDs.pop();
}

MMMEngine::ObjectManager::~ObjectManager()
{
    ShutDown();
}
