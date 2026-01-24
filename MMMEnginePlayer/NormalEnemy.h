#pragma once
#include "Enemy.h"

namespace MMMEngine {
	class NormalEnemy : public Enemy
	{
	protected:
		void Configure() override
		{
			HP = 30;
			atk = 2;
			velocity = 16.0f;
			playercheckdist = 12.0f;
			battledist = 4.0f;
			attackDelay = 1.2f;
		}
	};
}
