#include "SceneManager.h"
#include "SceneSerializer.h"

#include "json/json.hpp"
#include <fstream>
#include <filesystem>

DEFINE_SINGLETON(MMMEngine::SceneManager)

void MMMEngine::SceneManager::LoadScenes(std::wstring rootPath)
{
	//std::ifstream file(rootPath, std::ios::binary);
	//if (!file.is_open())
	//    return;

	//std::vector<uint8_t> buffer((std::istreambuf_iterator<char>(file)),
	//    std::istreambuf_iterator<char>());
	//file.close();

	//nlohmann::json snapshot = nlohmann::json::from_msgpack(buffer);

	//m_scenes.emplace_back(std::move(std::make_unique<Scene>()));
	//
	//auto& currentScene = *m_scenes[0].get();
	//currentScene.m_snapshot = snapshot;
	//SceneSerializer::Get().Deserialize(currentScene,currentScene.m_snapshot);
}

void MMMEngine::SceneManager::CreateEmptyScene()
{
	m_scenes.push_back(std::make_unique<Scene>());
	m_currentSceneID = 0;
}


const MMMEngine::SceneRef MMMEngine::SceneManager::GetCurrentScene() const
{
	return { m_currentSceneID , false };
}

void MMMEngine::SceneManager::RegisterGameObjectToDDOL(ObjPtr<GameObject> go)
{
	if (m_dontDestroyOnLoadScene)
	{
		m_dontDestroyOnLoadScene->RegisterGameObject(go);
		go->SetScene({ static_cast<size_t>(-1),true });
	}
#ifdef _DEBUG
	else
		assert(true && "Dont Destroy On Load 씬이 존재하지 않습니다.");
#endif;
}

MMMEngine::Scene* MMMEngine::SceneManager::GetSceneRaw(const SceneRef& ref)
{
	if (m_scenes.size() <= ref.id && !ref.id_DDOL)
		return nullptr;

	if (ref.id_DDOL)
		return m_dontDestroyOnLoadScene.get();

	return m_scenes[ref.id].get();
}

MMMEngine::SceneRef MMMEngine::SceneManager::GetSceneRef(const Scene* pScene)
{
	size_t idx = 0;
	for (const auto& uniqueP : m_scenes)
	{
		if (uniqueP.get() == pScene)
		{
			return SceneRef{ idx,false };
		}
		++idx;
	}

	return SceneRef{ static_cast<size_t>(-1),false };
}

void MMMEngine::SceneManager::ChangeScene(const std::string& name)
{
	auto it = m_sceneNameToID.find(name);
	if (it != m_sceneNameToID.end())
	{
		m_nextSceneID = it->second;
	}
}

void MMMEngine::SceneManager::ChangeScene(const size_t& id)
{
	if (id < m_scenes.size())
		m_nextSceneID = id;
}

void MMMEngine::SceneManager::StartUp(std::wstring rootPath, bool allowEmptyScene)
{
	m_dontDestroyOnLoadScene = std::make_unique<Scene>();

	// 고정된 경로로 json 바이너리를 읽고 씬파일경로를 불러오 ID맵을 초기화하고 초기씬을 생성함
	LoadScenes(rootPath);

	if (allowEmptyScene)
	{
		CreateEmptyScene();
	}
	else if(m_scenes.empty())
	{
		assert(false && "씬리스트가 비어있습니다!, 초기씬 로드에 실패했습니다!");
	}
}


void MMMEngine::SceneManager::ShutDown()
{
	m_dontDestroyOnLoadScene.reset();
	m_scenes.clear();
}

bool MMMEngine::SceneManager::CheckSceneIsChanged()
{
	if (m_nextSceneID != static_cast<size_t>(-1) &&
		m_nextSceneID < m_scenes.size())
	{
		if (m_currentSceneID != static_cast<size_t>(-1) && 
			m_currentSceneID < m_scenes.size())
		 	m_scenes[m_currentSceneID]->Clear();

		m_currentSceneID = m_nextSceneID;
		m_nextSceneID = static_cast<size_t>(-1);

		// todo : SceneSerializer 호출
		//m_currentSceneID->Initialize();
		return true;
	}

	return false;
}

MMMEngine::ObjPtr< MMMEngine::GameObject> MMMEngine::SceneManager::FindFromAllScenes(const std::string& name)
{
	for (auto& scene : m_scenes)
	{
		const auto& gameobjs_cache = scene->GetGameObjects();
		for (auto& go : gameobjs_cache)
		{
			if (go->GetName() == name)
			{
				return go;
			}
		}
	}
	const auto& ddol_gameobjs_cache = m_dontDestroyOnLoadScene->GetGameObjects();
	for (auto& ddol_go : ddol_gameobjs_cache)
	{
		if (ddol_go->GetName() == name)
		{
			return ddol_go;
		}
	}

	return nullptr;
}

MMMEngine::ObjPtr< MMMEngine::GameObject> MMMEngine::SceneManager::FindWithTagFromAllScenes(const std::string& tag)
{
	for (auto& scene : m_scenes)
	{
		const auto& gameobjs_cache = scene->GetGameObjects();
		for (auto& go : gameobjs_cache)
		{
			if (go->GetTag() == tag)
			{
				return go;
			}
		}
	}
	const auto& ddol_gameobjs_cache = m_dontDestroyOnLoadScene->GetGameObjects();
	for (auto& ddol_go : ddol_gameobjs_cache)
	{
		if (ddol_go->GetTag() == tag)
		{
			return ddol_go;
		}
	}

	return nullptr;
}

std::vector<MMMEngine::ObjPtr< MMMEngine::GameObject>> MMMEngine::SceneManager::FindGameObjectsWithTagFromAllScenes(const std::string& tag)
{
	std::vector<ObjPtr<GameObject>> cache;

	for (auto& scene : m_scenes)
	{
		const auto& gameobjs_cache = scene->GetGameObjects();
		for (auto& go : gameobjs_cache)
		{
			if (go->GetTag() == tag)
			{
				cache.push_back(go);
			}
		}
	}
	const auto& ddol_gameobjs_cache = m_dontDestroyOnLoadScene->GetGameObjects();
	for (auto& ddol_go : ddol_gameobjs_cache)
	{
		if (ddol_go->GetTag() == tag)
		{
			cache.push_back(ddol_go);
		}
	}

	return cache;
}
