#include "SnowballManager.h"
#include "MMMInput.h"
#include "MMMTime.h"
#include "Snowball.h"
#include "Player.h"
#include "Transform.h"

void MMMEngine::SnowballManager::Initialize()
{
	player = GameObject::Find("Player");
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
	if (!player) return;
	auto pc = player->GetComponent<Player>();
	if (!pc) return;

	if (Input::GetKeyUp(KeyCode::Space))
	{
		if (matchedSnowball)
		{
			auto sc = matchedSnowball->GetComponent<Snowball>();
			if (sc)
			{
				sc->controlled = false;
			}
			matchedSnowball = nullptr;
			pc->VelocityReturn();
		}
	}
	if (Input::GetKey(KeyCode::Space))
	{
		if (matchedSnowball)
		{
			auto scMe = matchedSnowball->GetComponent<Snowball>();
			auto matchedsnowpos = matchedSnowball->GetTransform()->GetWorldPosition();
			if (!scMe)
			{
				matchedSnowball = nullptr;
				spawnTimer = 0.0f;
			}
			else
			{
				pc->VelocityDown(scMe->GetScale());

				float myR = scMe->GetScale();

				for (size_t i = 0; i < Snows.size(); ++i)
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
			}
		}
		else
		{
			if (Input::GetKeyDown(KeyCode::Space))
			{
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
				if (pc->IsMoving())
				{
					spawnTimer += Time::GetDeltaTime();
					if (spawnTimer >= spawnDelay)
					{
						auto newSnow = NewObject<GameObject>();
						newSnow->AddComponent<Snowball>();
						newSnow->SetTag("Snowball");
						Snows.push_back(newSnow);
						ObjPtr<GameObject> created = newSnow;

						if (created)
						{
							auto sc = created->GetComponent<Snowball>();
							auto snowpos = created->GetTransform()->GetWorldPosition();
							auto snowtr = created->GetTransform();
							sc->controlled = true;
							auto pTr = player->GetTransform();
							auto playerpos = pTr->GetWorldPosition();
							auto pRot = pTr->GetWorldRotation();
							auto fwd = DirectX::SimpleMath::Vector3::Transform(
								DirectX::SimpleMath::Vector3::Forward, pRot);
							fwd.y = 0.0f;
							if (fwd.LengthSquared() > 1e-8f) fwd.Normalize();
							float offset = 1.5f;
							auto pos = playerpos + fwd * offset;
							snowtr->SetWorldPosition(pos);
							matchedSnowball = created;
						}
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