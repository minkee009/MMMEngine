#include "BehaviourManager.h"

DEFINE_SINGLETON(MMMEngine::BehaviourManager)

void MMMEngine::BehaviourManager::CheckAndSortBehaviours()
{
	if (m_needSort)
	{
		SortBehaviours();
		m_needSort = false;
	}
}

bool MMMEngine::BehaviourManager::StartUp(const std::string& userScriptsDLLPath)
{
	m_pScriptLoader = std::make_unique<ScriptLoader>();
	return m_pScriptLoader->LoadScriptDLL(userScriptsDLLPath);
}

void MMMEngine::BehaviourManager::ShutDown()
{
	m_pScriptLoader.release();
	m_activeBehaviours.clear();
	m_inactiveBehaviours.clear();
	m_pendingRegister.clear();
	m_pendingDestroy.clear();
	m_dirtyBehaviours.clear();
	m_pendingAwake.clear();
	m_pendingStart.clear();
	m_pendingOnEnable.clear();
	m_isInitializingPhase = false;
	m_isCollecting = false;
}

void MMMEngine::BehaviourManager::RegisterBehaviour(ObjPtr<Behaviour> behaviour)
{
	if (m_isInitializingPhase || m_isCollecting)
	{
		m_pendingRegister.push_back(behaviour);
		m_firstCallBehaviours.insert(behaviour);
		return;
	}

	m_inactiveBehaviours.push_back(behaviour);
	m_firstCallBehaviours.insert(behaviour);
	m_needSort = true; // Behaviour 정렬이 필요함을 표시
}

void MMMEngine::BehaviourManager::UnRegisterBehaviour(ObjPtr<Behaviour> behaviour)
{
	if (!behaviour.IsValid())
		return;

	auto it = std::find(m_activeBehaviours.begin(), m_activeBehaviours.end(), behaviour);
	if (it != m_activeBehaviours.end())
	{
		m_activeBehaviours.erase(it);
	}
	else
	{
		it = std::find(m_inactiveBehaviours.begin(), m_inactiveBehaviours.end(), behaviour);
		if (it != m_inactiveBehaviours.end())
		{
			m_inactiveBehaviours.erase(it);
		}
	}

	if (std::find(m_pendingDestroy.begin(), m_pendingDestroy.end(), behaviour) == m_pendingDestroy.end())
		m_pendingDestroy.push_back(behaviour);

	if (!m_pendingRegister.empty())
	{
		auto pit = std::find(m_pendingRegister.begin(), m_pendingRegister.end(), behaviour);
		if (pit != m_pendingRegister.end())
			m_pendingRegister.erase(pit);
	}
	m_pendingAwake.erase(behaviour);
	m_pendingStart.erase(behaviour);
	m_pendingOnEnable.erase(behaviour);
	m_firstCallBehaviours.erase(behaviour);
	m_needSort = true; // Behaviour 정렬이 필요함을 표시
}

void MMMEngine::BehaviourManager::SortBehaviours()
{
	std::sort(m_activeBehaviours.begin(), m_activeBehaviours.end(),
		[](ObjPtr<Behaviour> a, ObjPtr<Behaviour> b) {
			return a->m_executionOrder < b->m_executionOrder;
		});
}

void MMMEngine::BehaviourManager::AllSortBehaviours()
{
	std::sort(m_activeBehaviours.begin(), m_activeBehaviours.end(),
		[](ObjPtr<Behaviour> a, ObjPtr<Behaviour> b) {
			return a->m_executionOrder < b->m_executionOrder;
		});

	std::sort(m_inactiveBehaviours.begin(), m_inactiveBehaviours.end(),
		[](ObjPtr<Behaviour> a, ObjPtr<Behaviour> b) {
			return a->m_executionOrder < b->m_executionOrder;
		});
}

void MMMEngine::BehaviourManager::InitializeBehaviours()
{
	if (m_isInitializingPhase)
		return;

	m_isInitializingPhase = true;
	m_pendingAwake.clear();
	m_pendingStart.clear();
	m_pendingOnEnable.clear();

	struct CollectScope
	{
		bool& flag;
		CollectScope(bool& f) : flag(f) { flag = true; }
		~CollectScope() { flag = false; }
	} scope(m_isCollecting);

	// 활성화된 Behaviour를 모으는 컨테이너.
// count() 메서드로 O(1)에 존재 여부 확인 가능.
	std::unordered_set<ObjPtr<Behaviour>> changedBehavioursSet;
	std::unordered_set<ObjPtr<Behaviour>> newBehavioursSet;

	// m_inactiveBehaviours를 순회하며 활성화 객체 처리
	auto it = m_inactiveBehaviours.begin();
	while (it != m_inactiveBehaviours.end())
	{
		ObjPtr<Behaviour> currentBehaviour = *it;
		if (!currentBehaviour.IsValid() || currentBehaviour->IsDestroyed())
		{
			it = m_inactiveBehaviours.erase(it);
			continue;
		}

		if (currentBehaviour->IsActiveAndEnabled())
		{
			m_activeBehaviours.push_back(currentBehaviour);
			changedBehavioursSet.insert(currentBehaviour); // OnEnable 호출을 위해 추가

			// m_firstCallBehaviours에서 찾고, newBehavioursSet에 추가 및 m_firstCallBehaviours에서 제거
			if (m_firstCallBehaviours.erase(currentBehaviour) > 0) // 요소가 성공적으로 제거되면
			{
				newBehavioursSet.insert(currentBehaviour); // 새로운 Behaviour로 간주
			}
			m_needSort = true;

			// m_inactiveBehaviours에서 현재 요소를 제거
			it = m_inactiveBehaviours.erase(it);
		}
		else
		{
			++it;
		}
	}

	for (auto& behaviour : m_activeBehaviours)
	{
		if (newBehavioursSet.count(behaviour) > 0)
		{
			m_pendingAwake.insert(behaviour);
			m_pendingStart.insert(behaviour);
		}
	}

	for (auto& behaviour : m_activeBehaviours)
	{
		if (changedBehavioursSet.count(behaviour) > 0)
		{
			m_pendingOnEnable.insert(behaviour);
		}
	}
}

void MMMEngine::BehaviourManager::ExecuteAwake()
{
	if (!m_isInitializingPhase)
		return;

	for (auto& behaviour : m_activeBehaviours)
	{
		if (!behaviour.IsValid() || behaviour->IsDestroyed())
			continue;
		if (m_pendingAwake.count(behaviour) > 0)
			behaviour->CallMessage("Awake");
	}
}

void MMMEngine::BehaviourManager::ExecuteStart()
{
	if (!m_isInitializingPhase)
		return;

	for (auto& behaviour : m_activeBehaviours)
	{
		if (!behaviour.IsValid() || behaviour->IsDestroyed())
			continue;
		if (m_pendingStart.count(behaviour) > 0)
			behaviour->CallMessage("Start");
	}
}

void MMMEngine::BehaviourManager::ExecuteOnEnable()
{
	if (!m_isInitializingPhase)
		return;

	for (auto& behaviour : m_activeBehaviours)
	{
		if (!behaviour.IsValid() || behaviour->IsDestroyed())
			continue;
		if (m_pendingOnEnable.count(behaviour) > 0)
			behaviour->CallMessage("OnEnable");
	}
}

void MMMEngine::BehaviourManager::ClearInitializeCache()
{
	if (!m_isInitializingPhase)
		return;

	m_pendingAwake.clear();
	m_pendingStart.clear();
	m_pendingOnEnable.clear();

	if (!m_pendingRegister.empty())
	{
		for (auto& behaviour : m_pendingRegister)
			m_inactiveBehaviours.push_back(behaviour);
		m_pendingRegister.clear();
		m_needSort = true;
	}

	m_isInitializingPhase = false;
}

void MMMEngine::BehaviourManager::DisableBehaviours(bool dispatchMessages)
{
	if (m_dirtyBehaviours.empty() && m_pendingDestroy.empty())
		return;

	std::unordered_set<ObjPtr<Behaviour>> disableSeen;
	std::unordered_set<ObjPtr<Behaviour>> destroySeen;
	std::vector<ObjPtr<Behaviour>> disableList;
	std::vector<ObjPtr<Behaviour>> destroyList;
	disableList.reserve(m_dirtyBehaviours.size() + m_pendingDestroy.size());
	destroyList.reserve(m_pendingDestroy.size());

	auto pushUnique = [](std::vector<ObjPtr<Behaviour>>& list,
		std::unordered_set<ObjPtr<Behaviour>>& seen,
		const ObjPtr<Behaviour>& behaviour)
	{
		if (!behaviour.IsValid())
			return;
		if (seen.insert(behaviour).second)
			list.push_back(behaviour);
	};

	for (auto& behaviour : m_dirtyBehaviours)
	{
		if (!behaviour.IsValid())
			continue;

		if (!behaviour->IsActiveAndEnabled() || behaviour->IsDestroyed())
		{
			auto it = std::find(m_activeBehaviours.begin(), m_activeBehaviours.end(), behaviour);
			if (it != m_activeBehaviours.end())
			{
				pushUnique(disableList, disableSeen, behaviour);
				if (behaviour->IsDestroyed())
					pushUnique(destroyList, destroySeen, behaviour);
				m_needSort = true;

				m_inactiveBehaviours.push_back(behaviour); // 바로 m_inactiveBehaviours로 이동
				m_activeBehaviours.erase(it); // m_activeBehaviours에서 제거
			}
		}
	}
	m_dirtyBehaviours.clear();

	for (auto& pending : m_pendingDestroy)
	{
		pushUnique(disableList, disableSeen, pending);
		pushUnique(destroyList, destroySeen, pending);
	}
	m_pendingDestroy.clear();

	for (auto& behaviour : disableList)
	{
		if (dispatchMessages && behaviour.IsValid())
			behaviour->CallMessage("OnDisable");
	}

	for (auto& behaviour : destroyList)
	{
		if (dispatchMessages && behaviour.IsValid())
			behaviour->CallMessage("OnDestroy");
	}

}

void MMMEngine::BehaviourManager::MarkBehaviourDirty(ObjPtr<Behaviour> behaviour)
{
	if (!behaviour.IsValid())
		return;
	m_dirtyBehaviours.insert(behaviour);
}

void MMMEngine::BehaviourManager::BroadCastBehaviourMessage(const std::string& messageName)
{
	for (auto& behaviour : m_activeBehaviours)
	{
		behaviour->CallMessage(messageName);
	}
}

bool MMMEngine::BehaviourManager::ReloadUserScripts(const std::string& name)
{
	return m_pScriptLoader->LoadScriptDLL(name);
}

void MMMEngine::BehaviourManager::UnloadUserScripts()
{
	// 스크립트 언로드 시도
	if (m_pScriptLoader)
	{
		switch (m_pScriptLoader->UnloadScript())
		{
		case UnloadState::ScriptNotLoaded:   std::cout << "Script Not Loaded\n";   break;
		case UnloadState::UnloadFail:        std::cout << "Script Unload Fail\n";  break;
		case UnloadState::UnloadSuccess:     std::cout << "Script Unload Success!!\n"; break;
		}
	}

	// Behaviour 컨테이너 싹 비우기
	m_activeBehaviours.clear();
	m_inactiveBehaviours.clear();
	m_firstCallBehaviours.clear();
	m_pendingRegister.clear();
	m_pendingDestroy.clear();
	m_dirtyBehaviours.clear();
	m_pendingAwake.clear();
	m_pendingStart.clear();
	m_pendingOnEnable.clear();
	m_isInitializingPhase = false;
	m_isCollecting = false;
	m_needSort = false;
}

