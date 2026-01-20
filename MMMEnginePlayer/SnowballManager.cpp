#include "SnowballManager.h"
#include "MMMInput.h"
#include "MMMTime.h"
#include "Snowball.h"
#include "Player.h"

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
		}
	}
	if (Input::GetKey(KeyCode::Space))
	{
		if (matchedSnowball)
		{
			auto scMe = matchedSnowball->GetComponent<Snowball>();
			if (!scMe)
			{
				matchedSnowball = nullptr;
				spawnTimer = 0.0f;
			}
			else
			{
				pc->VelocityDown(scMe->GetScale());

				float myX = scMe->posX, myY = scMe->posY;
				float myR = scMe->GetScale();

				for (size_t i = 0; i < Snows.size(); ++i)
				{
					auto& otherObj = Snows[i];
					if (!otherObj) continue;
					if (otherObj == matchedSnowball) continue;

					auto scOther = otherObj->GetComponent<Snowball>();
					if (!scOther) continue;
					if (scOther->controlled) continue;

					float dx = scOther->posX - myX;
					float dy = scOther->posY - myY;
					float sumR = myR + scOther->GetScale();

					if (dx * dx + dy * dy <= sumR * sumR)
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

					float dx = sc->posX - pc->posX;
					float dy = sc->posY - pc->posY;
					float d2 = dx * dx + dy * dy;

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

							sc->posX = pc->posX;
							sc->posY = pc->posY;
							sc->controlled = true;

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