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
	if (player)
	{
		playercomp = player->GetComponent<Player>();
		playertr = player->GetTransform();
	}
	castle = GameObject::Find("Castle");
	if (castle)
	{
		castlecomp = castle->GetComponent<Castle>();
		castletr = castle->GetTransform();
	}
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
	if (!playertr || !castletr || !playercomp || !playertr) return;
	auto castlepos = castle->GetTransform()->GetWorldPosition();

	

}

void MMMEngine::SnowballManager::RemoveFromList(ObjPtr<GameObject> obj)
{
	auto it = std::find(Snows.begin(), Snows.end(), obj);
	if (it != Snows.end()) Snows.erase(it);
}