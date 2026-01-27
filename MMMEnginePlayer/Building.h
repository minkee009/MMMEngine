#pragma once
#include "ScriptBehaviour.h"
#include "rttr/type"
#include "Export.h"

namespace MMMEngine {
	class MMMENGINE_API Building : public ScriptBehaviour
	{
		RTTR_ENABLE(ScriptBehaviour)
		RTTR_REGISTRATION_FRIEND
	public:
		Building()
		{
			REGISTER_BEHAVIOUR_MESSAGE(Start)
			REGISTER_BEHAVIOUR_MESSAGE(Update)
		}
		void Initialize() override;
		void UnInitialize() override;
		void Start();
		void Update();
		void GetDamage(int t) { HP - t; HP = std::max(HP, 0); };
		void PointUp(float t);
	private:
		int HP = 50;
		int point = 0;
		int exp = 0;
		int atk = 10;
	};
}
