#pragma once
#include <unordered_set>
#include <string>
#include <queue>
#include <algorithm>

#include "ExportSingleton.hpp"
#include "Behaviour.h"
#include "ScriptLoader.h"

namespace MMMEngine
{
	class MMMENGINE_API BehaviourManager : public Utility::ExportSingleton<BehaviourManager>
	{
	private:
		friend class Behaviour;

		bool m_isInitializingPhase = false; // 초기화 파이프라인 진행 중인지
		bool m_isCollecting = false; // InitializeBehaviours 수집 중인지S

		std::vector<ObjPtr<Behaviour>> m_pendingRegister; // 초기화 중 등록된 Behaviour
		std::vector<ObjPtr<Behaviour>> m_pendingDestroy; // 파괴 예약된 Behaviour (OnDisable/OnDestroy 루프용)
		std::unordered_set<ObjPtr<Behaviour>> m_dirtyBehaviours; // 활성/비활성 상태 변경 감지용
		std::unordered_set<ObjPtr<Behaviour>> m_pendingAwake;
		std::unordered_set<ObjPtr<Behaviour>> m_pendingStart;
		std::unordered_set<ObjPtr<Behaviour>> m_pendingOnEnable;

		bool m_needSort = false; // Behaviour 정렬이 필요한지 여부
		std::vector<ObjPtr<Behaviour>> m_activeBehaviours; // 활성화된 Behaviour를 저장하는 벡터
		std::vector<ObjPtr<Behaviour>> m_inactiveBehaviours; // 비활성화된 Behaviour를 저장하는 벡터
		std::unordered_set<ObjPtr<Behaviour>> m_firstCallBehaviours;
		std::unique_ptr<ScriptLoader> m_pScriptLoader;

		// Behaviour를 등록하는 함수
		void RegisterBehaviour(ObjPtr<Behaviour> behaviour);

		// Behaviour를 제거하는 함수
		void UnRegisterBehaviour(ObjPtr<Behaviour> behaviour);

	public:
		// ExcutionOrder에 따라 Behaviour를 정렬하는 함수
		void SortBehaviours();
		void AllSortBehaviours();

		// 초기화 대상 수집 (Awake/Start/OnEnable 호출 전 단계)
		void InitializeBehaviours();
		void ExecuteAwake();
		void ExecuteStart();
		void ExecuteOnEnable();
		void ClearInitializeCache();

		// 비활성화된 Behaviour를 감지하는 함수
		void DisableBehaviours(bool dispatchMessages = true);
		void MarkBehaviourDirty(ObjPtr<Behaviour> behaviour);

		void BroadCastBehaviourMessage(const std::string& messageName);

		// 매개변수 있는 브로드캐스트
		template<typename... Args>
		void BroadCastBehaviourMessage(const std::string& messageName, Args&&... args)
		{
			for (auto& behaviour : m_activeBehaviours)
			{
				behaviour->CallMessage(messageName, std::forward<Args>(args)...);
			}
		}

		// 매개변수 있는 브로드캐스트
		template<typename... Args>
		void SpecificBroadCastBehaviourMessage(ObjPtr<GameObject>& obj, const std::string& messageName, Args&&... args)
		{
			for (auto& behaviour : m_activeBehaviours)
			{
				if (behaviour.IsValid() && behaviour->GetGameObject() == obj)
					behaviour->CallMessage(messageName, std::forward<Args>(args)...);
			}
		}


		bool ReloadUserScripts(const std::string& name);
		void UnloadUserScripts();

		template<typename... Args>
		void AllBroadCastBehaviourMessage(const std::string& messageName, Args&&... args)
		{
			for (auto& behaviour : m_activeBehaviours)
			{
				behaviour->CallMessage(messageName, std::forward<Args>(args)...);
			}
			for (auto& behaviour : m_inactiveBehaviours)
			{
				behaviour->CallMessage(messageName, std::forward<Args>(args)...);
			}
		}

		void CheckAndSortBehaviours();
		bool StartUp(const std::string& userScriptsDLLPath);
		void ShutDown();
	};
}
