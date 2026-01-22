#pragma once
#include "ScriptBehaviour.h"
#include "MMMApplication.h"

namespace MMMEngine {
	class Snowball : public ScriptBehaviour
	{
	public:
		void Initialize() override;
		void UnInitialize() override;
		void Update();
		void AssembleSnow(ObjPtr<GameObject> other);
		float GetScale() const { return scale; };
		bool controlled = false;
	private:
		void RollSnow();
		float scale = 0.01f;
		ObjPtr<GameObject> player;
	};
}