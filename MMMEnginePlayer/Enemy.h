#pragma once
#include "ScriptBehaviour.h"
#include "Player.h"

namespace MMMEngine {
	class Enemy : public ScriptBehaviour
	{
	public:
		void Initialize() {};
		void Update();
	private:
		bool playerFind = false;
		float forwardAngle = 0.0f;
		float velocity = 50.0f;
		float posX = 100.0f;
		float posY = 100.0f;
		float attackTimer = 1.0f;
	};
}

