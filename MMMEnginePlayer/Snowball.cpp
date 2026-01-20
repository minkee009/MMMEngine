#include "Snowball.h"
#include "MMMTime.h"
#include "Player.h"

void MMMEngine::Snowball::Initialize()
{
	player = GameObject::Find("Player");
}

void MMMEngine::Snowball::Update()
{
	if (controlled)
		RollSnow();
}

void MMMEngine::Snowball::RollSnow()
{
	if(scale <= 10.0f || player->GetComponent<Player>()->IsMoving())
		scale += Time::GetDeltaTime();
	posX = player->GetComponent<Player>()->posX;
	posY = player->GetComponent<Player>()->posY;
}

void MMMEngine::Snowball::AssembleSnow(ObjPtr<GameObject> other)
{
	auto snowcomp = other->GetComponent<Snowball>();
	if (snowcomp == nullptr)
		return;
	if (snowcomp->scale > 10.0f - scale)
		scale = 10.0f;
	else
		scale += snowcomp->scale;
	Destroy(other);
}