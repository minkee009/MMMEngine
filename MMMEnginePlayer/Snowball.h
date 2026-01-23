#pragma once
#include "ScriptBehaviour.h"
#include "MMMApplication.h"
#include "SimpleMath.h"

namespace MMMEngine {
	class Transform;
	class Snowball : public ScriptBehaviour
	{
	public:
		void Initialize() override;
		void UnInitialize() override;
		void Update();
		void AssembleSnow(ObjPtr<GameObject> other);
		float GetScale() const { return scale; };
		void SetControlled(bool v) { controlled = v; }
		bool GetControlled() { return controlled; }
	private:
		void RollSnow();
		float scale = 0.01f;
		float maxscale = 10.0f;
		float offset = 1.5f; //눈과 플레이어간의 거리
		float velocity = 25.0f;
		bool controlled = false;
		ObjPtr<Transform> tr;
		DirectX::SimpleMath::Vector3 pos;
		ObjPtr<GameObject> player;
		ObjPtr<Transform> playertr;
		DirectX::SimpleMath::Vector3 playerpos;
		DirectX::SimpleMath::Quaternion playerrot;
	};
}