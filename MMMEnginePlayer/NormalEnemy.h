#pragma once
#include "Enemy.h"

namespace MMMEngine {
	class MMMENGINE_API NormalEnemy : public Enemy
	{
		RTTR_ENABLE(Enemy)
		RTTR_REGISTRATION_FRIEND
	public:
		NormalEnemy()
		{
			REGISTER_BEHAVIOUR_MESSAGE(Start)
			REGISTER_BEHAVIOUR_MESSAGE(Update)
		}
	protected:
		void Configure() override
		{
			HP = 30;
			atk = 2;
			velocity = 13.0f;
			playercheckdist = 12.0f;
			battledist = 1.7f;
			attackDelay = 0.65f;
		}
	};
}
