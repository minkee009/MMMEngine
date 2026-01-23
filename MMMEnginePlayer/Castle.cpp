#include "Castle.h"
#include "MMMTime.h"

void MMMEngine::Castle::Initialize()
{

}

void MMMEngine::Castle::UnInitialize()
{

}

void MMMEngine::Castle::Update()
{
	AutoHeal();
}

void MMMEngine::Castle::AutoHeal()
{
	if (prevHP > HP)
	{
		fighting = true;
		NonfightTimer = 0.0f;
	}
	prevHP = HP;
	if (fighting)
	{
		NonfightDelay += Time::GetDeltaTime();
		if (NonfightTimer >= NonfightDelay)
		{
			fighting = false;
			healTimer = 0.0f;
		}
	}
	else if (HP < maxHP)
	{
		healTimer += Time::GetDeltaTime();
		if (healTimer >= healDelay)
		{
			HP = std::min(HP + 3, maxHP);
			healTimer = 0.0f;
		}
	}
}

void MMMEngine::Castle::CoinUp(float t)
{
	coin += static_cast<int>(t);
}