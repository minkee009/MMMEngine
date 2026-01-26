#include "GameManager.h"
#include "Player.h"
#include "Castle.h"
#include "MMMTime.h"
#include "Transform.h"
#include "EnemySpawner.h"
#include "MeshRenderer.h"

void MMMEngine::GameManager::Initialize()
{
	auto mesh = ResourceManager::Get().Load<StaticMesh>(L"Assets/Castle_StaticMesh.staticmesh");
	castle = NewObject<GameObject>();
	castle->SetTag("Castle");
	castle->AddComponent<Castle>();
	castle->AddComponent<MeshRenderer>();
	castle->GetComponent<MeshRenderer>()->SetMesh(mesh);
	castle->GetTransform()->SetWorldPosition(0, 0, 0);
	player = NewObject<GameObject>();
	player->SetTag("Player");
	player->AddComponent<Player>();
	player->AddComponent<MeshRenderer>();
	player->GetComponent<MeshRenderer>()->SetMesh(mesh);
	player->GetTransform()->SetWorldPosition(0, 0, -10);

	instance = GetGameObject()->GetComponent<GameManager>();
}

void MMMEngine::GameManager::UnInitialize()
{
	player = nullptr;
	castle = nullptr;

	instance = nullptr;
}

void MMMEngine::GameManager::Update()
{
	auto playercomp = player->GetComponent<Player>();
	auto castlecomp = castle->GetComponent<Castle>();

	if (nowSetting)
	{
		settingTimer += Time::GetDeltaTime();
		if (settingTimer >= settingfullTime)
		{
			nowSetting = false;
			settingTimer = 0.0f;
		}
	}
	else
	{
		NormalSpawnTimer += Time::GetDeltaTime();
		if (NormalSpawnTimer >= NormalSpawnDelay) {
			EnemySpawner::instance->MakeNormalEnemy();
			NormalSpawnTimer = 0.0f;
		}
	}



	if (playercomp->PlayerDeath() || castlecomp->CastleDeath())
	{
		GameOver = true;
	}
}