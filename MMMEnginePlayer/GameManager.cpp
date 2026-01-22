#include "GameManager.h"
#include "Player.h"
#include "Castle.h"

void MMMEngine::GameManager::Initialize()
{
	player = GameObject::Find("Player");
	castle = GameObject::Find("Castle");
}

void MMMEngine::GameManager::UnInitialize()
{
	player = nullptr;
	castle = nullptr;
}

void MMMEngine::GameManager::Update()
{
	auto playercomp = player->GetComponent<Player>();
	auto castlecomp = castle->GetComponent<Castle>();

	if (playercomp->PlayerDeath() || castlecomp->CastleDeath())
	{
		GameOver = true;
	}
}