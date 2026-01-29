#pragma once
#include "Export.h"
#include "ExportSingleton.hpp"
#include "Object.h"
#include <vector>
#include <queue>
#include <mutex>

namespace MMMEngine
{
    class MMMENGINE_API ObjectManager : public Utility::ExportSingleton<ObjectManager>
    {
    private:

        struct ObjectPtrInfo
        {
            Object* raw = nullptr;
            uint32_t ptrGenerations = 0;

            float destroyRemainTime = -1.0f;
            bool destroyScheduled = false;  
        };

        std::vector<ObjectPtrInfo> m_objectPtrInfos;
        std::queue<uint32_t> m_freePtrIDs;

        std::vector<uint32_t> m_delayedDestroy;   //파괴 예약 ID
        std::vector<uint32_t> m_pendingDestroy;   //완전 파괴 ID

    public:
        static bool IsCreatingObject();
        static bool IsDestroyingObject();

        // RAII 스코프 -> Object 스택 생성 막기용
        class MMMENGINE_API CreationScope
        {
        public:
            CreationScope();
            ~CreationScope();
        };

        class MMMENGINE_API DestroyScope
        {
        public:
            DestroyScope();
            ~DestroyScope();
        };

        bool IsValidPtr(uint32_t ptrID, uint32_t generation, const void* ptr) const;

        // SelfPtr<T>의 빠른 구현을 위한 함수, 절대 외부 호출하지 말 것
        template<typename T>
        ObjPtr<T> GetPtrFast(Object* raw, uint32_t ptrID, uint32_t ptrGen)
        {
            return ObjPtr<T>(static_cast<T*>(raw), ptrID, ptrGen);
        }

        template<typename T>
        ObjPtr<T> GetPtr(uint32_t ptrID, uint32_t ptrGen)
        {
            if (ptrID >= m_objectPtrInfos.size())
                return ObjPtr<T>();

            Object* obj = m_objectPtrInfos[ptrID].raw;
            if (!obj || obj->IsDestroyed())
                return ObjPtr<T>();

#ifndef NDEBUG
            T* typedObj = dynamic_cast<T*>(obj);
            assert(typedObj && "GetPtr<T>: 타입 불일치! ptrID에 들어있는 실제 타입을 확인하세요.");
#endif
            if(m_objectPtrInfos[ptrID].ptrGenerations != ptrGen)
                return ObjPtr<T>();

            return ObjPtr<T>(static_cast<T*>(obj), ptrID, ptrGen);
        }


		// 엔진 코어에서 사용할 수 있는 헬퍼 함수 ( 절대 외부 호출하지 말 것 )
        template<typename T>
        ObjPtr<T> GetPtrFromRaw(void* raw)
        {
            if (!raw)
                return ObjPtr<T>();

			Object* obj = static_cast<Object*>(raw);
            uint32_t ptrID = obj->m_ptrID;
            uint32_t ptrGen = obj->m_ptrGen;
            return GetPtrFast<T>(obj, ptrID, ptrGen);
		}

        template<typename T>
        ObjPtr<T> FindObjectByType()
        {
            for (uint32_t i = 0; i < m_objectPtrInfos.size(); ++i)
            {
                auto& info = m_objectPtrInfos[i];
                if (!IsValidPtr(i, info.ptrGenerations, info.raw))
                    continue;

                auto obj = info.raw;
                if (T* castedObj = dynamic_cast<T*>(obj))
                {
                    return ObjPtr<T>(castedObj, i, info.ptrGenerations);
                }
            }

            return ObjPtr<T>();
        }

        template<typename T>
        std::vector<ObjPtr<T>> FindObjectsByType()
        {
            std::vector<ObjPtr<T>> objects;

            for (uint32_t i = 0; i < m_objectPtrInfos.size(); ++i)
            {
                auto& info = m_objectPtrInfos[i];
                if (!IsValidPtr(i, info.ptrGenerations, info.raw))
                    continue;

                auto obj = info.raw;
                if (T* castedObj = dynamic_cast<T*>(obj))
                {
                    objects.emplace_back(ObjPtr<T>(castedObj, i, info.ptrGenerations));
                }
            }

            return objects;
        }

        template<typename T, typename... Args>
        ObjPtr<T> NewObject(Args&&... args)
        {
            static_assert(std::is_base_of_v<Object, T>, "T는 반드시 Object를 상속받아야 합니다.");
            static_assert(!std::is_abstract_v<T>, "추상적인 Object는 만들 수 없습니다.");

            CreationScope scope;

            T* newObj = new T(std::forward<Args>(args)...);
            uint32_t ptrID;
            uint32_t ptrGen;

            if (m_freePtrIDs.empty())
            {
                // 새 슬롯 할당
                ptrID = static_cast<uint32_t>(m_objectPtrInfos.size());
                m_objectPtrInfos.push_back({ newObj,0,-1.0f,false });
                ptrGen = 0;
            }
            else
            {
                // 재사용 슬롯
                ptrID = m_freePtrIDs.front();
                m_freePtrIDs.pop();
                m_objectPtrInfos[ptrID].raw = newObj;
                ptrGen = ++m_objectPtrInfos[ptrID].ptrGenerations;
            }
            auto baseObj = static_cast<Object*>(newObj);
            baseObj->m_ptrID = ptrID;
            baseObj->m_ptrGen = ptrGen;

            newObj->Construct();
            return ObjPtr<T>(newObj, ptrID, ptrGen);
        }

        void Destroy(const ObjPtrBase& objPtr, float delayTime = 0.0f);

        void StartUp();
        void ShutDown();

        void UpdateInternalTimer(float deltaTime);
        void ProcessPendingDestroy();

        ObjectManager() = default;
        ~ObjectManager();
    };
} 
