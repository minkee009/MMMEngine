#include "Castle.h"
#include "MMMTime.h"

void MMMEngine::Castle::Update()
{
	if (!fighting && HP < 10)
	{
		healTimer -= Time::GetDeltaTime();
		if (healTimer <= 0.0f) {
			if (HP < 10 - 5)
				HP += 5;
			else
				HP = 10;
			healTimer = 1.0f;
		}
	}
	else
		healTimer = 1.0f;
}

void MMMEngine::Castle::CoinUp(float t)
{
	coin += static_cast<int>(t);
}