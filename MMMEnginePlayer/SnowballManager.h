#pragma once
#include "ScriptBehaviour.h"
#include <vector>

namespace MMMEngine {
	class Player;
	class Castle;
	class Transform;
	class SnowballManager : public ScriptBehaviour
	{
	public:
		void Initialize() override;
		void UnInitialize() override;
		void Update();
		//가까이 있는 눈 객체 찾기
		ObjPtr<GameObject> FindNearestSnowball(const DirectX::SimpleMath::Vector3& playerPos, float range)
		{
			ObjPtr<GameObject> nearest = nullptr;
			float bestD2 = range * range;

			for (auto& sObj : Snows)
			{
				if (!sObj) continue;
				auto sc = sObj->GetComponent<Snowball>();
				if (!sc) continue;
				if (sc->GetControlled()) continue;

				auto snowPos = sObj->GetTransform()->GetWorldPosition();
				float dx = snowPos.x - playerPos.x;
				float dz = snowPos.z - playerPos.z;
				float d2 = dx * dx + dz * dz;

				if (d2 < bestD2) {
					bestD2 = d2;
					nearest = sObj;
				}
			}
			return nearest;
		}

		std::vector<ObjPtr<GameObject>> Snows;

		static ObjPtr<SnowballManager> instance;
	private:
		void RemoveFromList(ObjPtr<GameObject> obj);
		//눈 생성
		ObjPtr<GameObject> MakeSnowball()
		{
			auto obj = NewObject<GameObject>();
			obj->AddComponent<Snowball>();
			obj->SetTag("Snowball");
			Snows.push_back(obj);
			return obj;
		}

		ObjPtr<GameObject> player;
		ObjPtr<Player> playercomp;
		ObjPtr<Transform> playertr;
		ObjPtr<GameObject> castle;
		ObjPtr<Castle> castlecomp;
		ObjPtr<Transform> castletr;

		float spawnTimer = 0.0f;
		float spawnDelay = 0.2f; //눈 생성 타이밍
		float CoinupRange = 1.0f; //성에 눈이 들어가는 거리
		float offset = 1.5f;  //눈 생성 시 플레이어와의 거리
	};
}

