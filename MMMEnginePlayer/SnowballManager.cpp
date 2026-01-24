#include "SnowballManager.h"
#include "MMMInput.h"
#include "MMMTime.h"
#include "Snowball.h"
#include "Player.h"
#include "Transform.h"
#include "Castle.h"

void MMMEngine::SnowballManager::Initialize()
{
	castle = GameObject::Find("Castle");
	if (castle)
	{
		castlecomp = castle->GetComponent<Castle>();
		castletr = castle->GetTransform();
	}
	instance = GetGameObject()->GetComponent<SnowballManager>();
}

void MMMEngine::SnowballManager::UnInitialize()
{
	for (auto& snow : Snows)
	{
		Destroy(snow);
	}
	Snows.clear();
	if (instance == GetGameObject()->GetComponent<SnowballManager>())
		instance = nullptr;
}

void MMMEngine::SnowballManager::Update()
{
	if (!castletr|| !castlecomp) return;
	castlepos = castle->GetTransform()->GetWorldPosition();

	AssembleSnow();
	SnowToCastle();
}

void MMMEngine::SnowballManager::OnScoopStart(Player& player)
{
	if (player.GetMatchedSnowball()) return;
	ObjPtr<GameObject> nearest = nullptr;
	float bestD2 = player.GetPickupRange() * player.GetPickupRange();
	auto playerPos = player.GetTransform()->GetWorldPosition();
	for (auto& sObj : Snows)
	{
		if (!sObj) continue;
		auto sc = sObj->GetComponent<Snowball>();
		if (!sc) continue;
		if (sc->IsCarried()) continue;

		auto snowPos = sObj->GetTransform()->GetWorldPosition();
		float dx = snowPos.x - playerPos.x;
		float dz = snowPos.z - playerPos.z;
		float d2 = dx * dx + dz * dz;

		if (d2 < bestD2) {
			bestD2 = d2;
			nearest = sObj;
		}
	}
	if (nearest)
	{
		nearest->GetComponent<Snowball>()->carrier = &player;
		player.AttachSnowball(nearest);
		scoopStates[&player] = {};
	}
	else
	{
		auto& st = scoopStates[&player];
		st.active = true;
		st.holdTime = 0.0f;
	}
}

void MMMEngine::SnowballManager::OnScoopHold(Player& player)
{
	// 이미 들고 있으면 생성할 이유 없음
	if (player.GetMatchedSnowball()) return;

	auto it = scoopStates.find(&player);
	if (it == scoopStates.end()) return;

	auto& st = it->second;
	if (!st.active) return;

	st.holdTime += Time::GetDeltaTime();
	if (st.holdTime < snowSpawnDelay) return;

	// 1) 생성 위치 계산: 플레이어 앞(현재 rot/yaw 기준)
	auto tr = player.GetTransform();
	if (!tr) return;

	auto playerPos = tr->GetWorldPosition();
	auto playerRot = tr->GetWorldRotation();

	auto fwd = DirectX::SimpleMath::Vector3::Transform(
		DirectX::SimpleMath::Vector3::Forward, playerRot);
	fwd.y = 0.0f;
	if (fwd.LengthSquared() > 1e-8f) fwd.Normalize();

	auto spawnPos = playerPos + fwd * player.GetPickupRange();

	// 2) 눈 생성 + 등록
	auto obj = NewObject<GameObject>();
	obj->AddComponent<Snowball>();
	obj->GetTransform()->SetWorldPosition(spawnPos);
	Snows.push_back(obj);

	// 3) 상태 전이(매니저 책임) + 플레이어 매칭
	if (auto sc = obj->GetComponent<Snowball>())
	{
		sc->carrier = &player;
	}
	player.AttachSnowball(obj);

	// 4) 홀드 상태 종료/리셋
	st.active = false;
	st.holdTime = 0.0f;
}

void MMMEngine::SnowballManager::OnScoopEnd(Player& player)
{
	// 홀드/스폰 상태 정리 (매칭 유무와 무관)
	auto it = scoopStates.find(&player);
	if (it != scoopStates.end())
	{
		it->second.active = false;
		it->second.holdTime = 0.0f;
	}

	auto sObj = player.GetMatchedSnowball();
	if (!sObj) return;

	auto sc = sObj->GetComponent<Snowball>();
	if (sc)
	{
		sc->carrier = nullptr;
	}

	player.DetachSnowball();
}

void MMMEngine::SnowballManager::RemoveFromList(ObjPtr<GameObject> obj)
{
	auto it = std::find(Snows.begin(), Snows.end(), obj);
	if (it != Snows.end()) Snows.erase(it);
}

void MMMEngine::SnowballManager::AssembleSnow()
{
	// 1) 메인 찾기
	ObjPtr<GameObject> mainObj = nullptr;
	ObjPtr<Snowball> mainSc = nullptr;

	for (auto& obj : Snows)
	{
		if (!obj) continue;

		auto sc = obj->GetComponent<Snowball>();
		if (!sc) continue;

		// carrier 기반 (네가 방금 추가한 것 활용)
		if (sc->IsCarried())   // 또는 sc->IsCarried()
		{
			mainObj = obj;
			mainSc = sc;
			break; // 싱글 기준: 들고 있는 눈은 하나라고 가정
		}
	}

	if (!mainObj || !mainSc) return;

	// 2) 메인 기준값 캐싱
	auto mainPos = mainObj->GetTransform()->GetWorldPosition();
	float mainR = mainSc->GetScale();

	// 3) 주변 비컨트롤 눈들 검사 + 합체
	for (int i = 0; i < (int)Snows.size(); ++i)
	{
		auto& otherObj = Snows[i];
		if (!otherObj) continue;
		if (otherObj == mainObj) continue;

		auto scOther = otherObj->GetComponent<Snowball>();
		if (!scOther) continue;
		if (scOther->IsCarried()) continue; // 컨트롤 중인 애는 대상 제외

		auto otherPos = otherObj->GetTransform()->GetWorldPosition();

		float dx = otherPos.x - mainPos.x;
		float dz = otherPos.z - mainPos.z;

		float otherR = scOther->GetScale();
		float sumR = mainR + otherR;

		if (dx * dx + dz * dz <= sumR * sumR)
		{
			RemoveFromList(otherObj);
			mainSc->EatSnow(otherObj);
			--i;
			mainR = mainSc->GetScale();
			// mainPos도 합체로 이동할 수 있으면 업데이트
			mainPos = mainObj->GetTransform()->GetWorldPosition();
		}
	}
}

void MMMEngine::SnowballManager::SnowToCastle()
{
	// 1) 메인 찾기
	ObjPtr<GameObject> mainObj = nullptr;
	ObjPtr<Snowball> mainSc = nullptr;

	for (auto& obj : Snows)
	{
		if (!obj) continue;

		auto sc = obj->GetComponent<Snowball>();
		if (!sc) continue;

		if (sc->IsCarried())
		{
			mainObj = obj;
			mainSc = sc;
			break;
		}
	}

	if (!mainObj || !mainSc) return;

	// 2) 거리 체크 (XZ 기준)
	auto mainPos = mainObj->GetTransform()->GetWorldPosition();

	float dx = mainPos.x - castlepos.x;
	float dz = mainPos.z - castlepos.z;
	float d2 = dx * dx + dz * dz;

	float r = CoinupRange; // 필요하면 mainR 더해서 판정 넓힐 수도 있음
	if (d2 > r * r) return;

	// 3) 코인 증가 (Castle 컴포넌트가 있다면)
	castlecomp->CoinUp(mainSc->GetScale());

	// 4) 매칭/캐리어 정리
	if (auto player = mainSc->carrier)
	{
		// 플레이어가 들고있던 matched를 풀어줌
		player->DetachSnowball();
		mainSc->carrier = nullptr; // 또는 mainSc->ClearCarrier()
	}

	// 5) 리스트 제거 + 파괴
	RemoveFromList(mainObj);
	Destroy(mainObj);
}