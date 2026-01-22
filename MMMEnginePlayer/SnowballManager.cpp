#include "SnowballManager.h"
#include "MMMInput.h"
#include "MMMTime.h"
#include "Snowball.h"
#include "Player.h"
#include "Transform.h"
#include "Castle.h"

void MMMEngine::SnowballManager::Initialize()
{
	player = GameObject::Find("Player");
	castle = GameObject::Find("Castle");
}

void MMMEngine::SnowballManager::UnInitialize()
{
	for (auto& snow : Snows)
	{
		Destroy(snow);
	}
	Snows.clear();
}

void MMMEngine::SnowballManager::Update()
{
	auto playercomp = player->GetComponent<Player>();
	auto castlecomp = castle->GetComponent<Castle>();
	auto castlepos = castle->GetTransform()->GetLocalPosition();

	if (Input::GetKeyUp(KeyCode::Space))
	{
		//space에서 손을 뗐을 때 매칭된 눈덩이가 있으면 매칭 해제 및 속도 원상복귀
		if (matchedSnowball)
		{
			auto sc = matchedSnowball->GetComponent<Snowball>();
			if (sc)
			{
				sc->controlled = false;
			}
			matchedSnowball = nullptr;
			playercomp->VelocityReturn();
		}
	}
	if (Input::GetKey(KeyCode::Space))
	{
		if (matchedSnowball)
		{
			auto scMe = matchedSnowball->GetComponent<Snowball>();
			auto matchedsnowpos = matchedSnowball->GetTransform()->GetWorldPosition();
			float dx = matchedsnowpos.x - castlepos.x;
			float dz = matchedsnowpos.z - castlepos.z;
			float d2 = dx * dx + dz * dz;
			if (!scMe)
			{
				matchedSnowball = nullptr;
				spawnTimer = 0.0f;
			}
			else
			{
				float myR = scMe->GetScale();
				//눈 사이즈에 따라 플레이어 감속
				playercomp->VelocityDown(myR);
				//눈덩이 합체 로직(눈 사이즈 설정에 따라 수식 조정)
				for (int i = 0; i < Snows.size(); ++i)
				{
					auto& otherObj = Snows[i];
					if (!otherObj) continue;
					if (otherObj == matchedSnowball) continue;

					auto scOther = otherObj->GetComponent<Snowball>();
					auto othersnowpos = otherObj->GetTransform()->GetWorldPosition();
					if (!scOther) continue;
					if (scOther->controlled) continue;

					float dx = othersnowpos.x - matchedsnowpos.x;
					float dz = othersnowpos.z - matchedsnowpos.z;
					float sumR = myR + scOther->GetScale();

					if (dx * dx + dz * dz <= sumR * sumR)
					{
						scMe->AssembleSnow(otherObj);
						Snows.erase(Snows.begin() + i);
						--i;
					}
				}
				float bestD2 = CoinupRange * CoinupRange;
				if (d2 < bestD2)
				{
					castlecomp->CoinUp(scMe->GetScale());
					auto it = std::find(Snows.begin(), Snows.end(), matchedSnowball);
					if (it != Snows.end())
					{
						Snows.erase(it);
					}
					Destroy(matchedSnowball);
					matchedSnowball = nullptr;
				}
				
			}
		}
		else
		{
			if (Input::GetKeyDown(KeyCode::Space))
			{
				//범위 안에 눈덩이가 있을 경우 해당 눈덩이를 매칭
				ObjPtr<GameObject> nearest;
				float bestD2 = pickupRange * pickupRange;

				for (auto& sObj : Snows)
				{
					auto sc = sObj->GetComponent<Snowball>();
					if (!sc) continue;
					if (sc->controlled) continue;
					auto snowpos = sObj->GetTransform()->GetWorldPosition();
					auto playerpos = player->GetTransform()->GetWorldPosition();
					float dx = snowpos.x - playerpos.x;
					float dz = snowpos.z - playerpos.z;
					float d2 = dx * dx + dz * dz;

					if (d2 < bestD2)
					{
						bestD2 = d2;
						nearest = sObj;
					}
				}
				if (nearest)
				{
					matchedSnowball = nearest;

					auto sc = matchedSnowball->GetComponent<Snowball>();
					sc->controlled = true;

					spawnTimer = 0.0f;
				}
			}
			if (!matchedSnowball)
			{
				if (playercomp->IsMoving())
				{
					//매칭된 눈덩이가 없는 상태로 이동 시 0.2초 뒤 눈덩이 생성 후 매칭
					spawnTimer += Time::GetDeltaTime();
					if (spawnTimer >= spawnDelay)
					{
						MakeSnowball();
						spawnTimer = 0.0f;
					}
				}
				else
				{
					spawnTimer = 0.0f;
				}
			}
		}
	}

}

void MMMEngine::SnowballManager::MakeSnowball()
{
	auto newSnow = NewObject<GameObject>();
	newSnow->AddComponent<Snowball>();
	newSnow->SetTag("Snowball");
	Snows.push_back(newSnow);
	ObjPtr<GameObject> created = newSnow;

	auto sc = created->GetComponent<Snowball>();
	auto snowtr = created->GetTransform();
	auto snowpos = snowtr->GetWorldPosition();
	sc->controlled = true;
	auto pTr = player->GetTransform();
	auto playerpos = pTr->GetWorldPosition();
	auto playerrot = pTr->GetWorldRotation();
	//눈덩이 위치 설정, 테스트 후 수정
	auto fwd = DirectX::SimpleMath::Vector3::Transform(
		DirectX::SimpleMath::Vector3::Forward, playerrot);
	fwd.y = 0.0f;
	if (fwd.LengthSquared() > 1e-8f) fwd.Normalize();
	auto pos = playerpos + fwd * offset;
	snowtr->SetWorldPosition(pos);

	matchedSnowball = created;
}