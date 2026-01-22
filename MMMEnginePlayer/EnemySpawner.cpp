#include "EnemySpawner.h"
#include "Enemy.h"
#include "Player.h"
#include "MMMTime.h"
#include "Transform.h"

void MMMEngine::EnemySpawner::Initialize()
{

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
	Enemys.erase(
		std::remove_if(Enemys.begin(), Enemys.end(),
			[](const ObjPtr<GameObject>& e)
			{
				return !e;
			}),
		Enemys.end()
	);

	EnemySpawnTimer -= Time::GetDeltaTime();
	if (EnemySpawnTimer <= 0.0f)
	{
		MakeEnemy();
		EnemySpawnTimer = 10.0f;
	}
}

void MMMEngine::EnemySpawner::MakeEnemy()
{
	auto newEnemy = NewObject<GameObject>();
	newEnemy->AddComponent<Enemy>();
	newEnemy->SetTag("Enemy");
	Enemys.push_back(newEnemy);
	ObjPtr<GameObject> created = newEnemy;

	auto enemycomp = created->GetComponent<Enemy>();
	auto enemytr = created->GetTransform();
	auto enemypos = enemytr->GetWorldPosition();
	
	enemypos.x = 50.f;
	enemypos.z = 50.f;

	enemytr->SetWorldPosition(enemypos);
}