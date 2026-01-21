#pragma once
#include "ScriptBehaviour.h"
#include "MMMApplication.h"

namespace MMMEngine {
	class Snowball : public ScriptBehaviour
	{
	public:
		void Initialize() override;
		void Update();
		void RollSnow();
		void AssembleSnow(ObjPtr<GameObject> other);
		float GetScale() const { return scale; };
		bool controlled = false;
	private:
		float scale = 0.01f;
		ObjPtr<GameObject> player;
	};
}