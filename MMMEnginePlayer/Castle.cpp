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
	if (prevHP > HP)
	{
		fighting = true;
		NonfightTimer = 5.0f;
	}
	prevHP = HP;
	if (fighting)
	{
		NonfightTimer -= Time::GetDeltaTime();
		if (NonfightTimer <= 0.0f)
		{
			fighting = false;
			healTimer = 1.0f;
		}
	}
	else if (HP < 10)
	{
		healTimer -= Time::GetDeltaTime();
		if (healTimer <= 0.0f)
		{
			HP = std::min(HP + 3, 10);
			healTimer = 1.0f;
		}
	}
}

void MMMEngine::Castle::CoinUp(float t)
{
	coin += static_cast<int>(t);
}