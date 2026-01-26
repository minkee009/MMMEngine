#include "EnemySpawner.h"
#include "Enemy.h"
#include "NormalEnemy.h"
#include "Player.h"
#include "MMMTime.h"
#include "Transform.h"
#include "MeshRenderer.h"

void MMMEngine::EnemySpawner::Initialize()
{
	instance = GetGameObject()->GetComponent<EnemySpawner>();
}

void MMMEngine::EnemySpawner::UnInitialize()
{
	for (auto& enemy : Enemys)
	{
		Destroy(enemy);// Destroy되면 nullptr로 바뀐다 가정
	}
	Enemys.clear();
}

void MMMEngine::EnemySpawner::Update()
{
	for (auto it = Enemys.begin(); it != Enemys.end(); )
	{
		if (!(*it))   // ObjPtr가 Destroy되면 nullptr로 바뀐다면
		{
			it = Enemys.erase(it);
			continue;
		}
	}
}

void MMMEngine::EnemySpawner::MakeNormalEnemy()
{
	auto newEnemy = NewObject<GameObject>();
	newEnemy->AddComponent<NormalEnemy>();
	newEnemy->SetTag("Enemy");
	newEnemy->AddComponent<MeshRenderer>();
	auto mesh = ResourceManager::Get().Load<StaticMesh>(L"Assets/Castle_StaticMesh.staticmesh");
	newEnemy->GetComponent<MeshRenderer>()->SetMesh(mesh);
	Enemys.push_back(newEnemy);
	ObjPtr<GameObject> created = newEnemy;

	auto enemycomp = created->GetComponent<Enemy>();
	auto enemytr = created->GetTransform();
	auto enemypos = enemytr->GetWorldPosition();
	
	//좌표는 임의로 설정. 추후 수정
	enemypos.x = 50.f;
	enemypos.z = 50.f;

	enemytr->SetWorldPosition(enemypos);
}