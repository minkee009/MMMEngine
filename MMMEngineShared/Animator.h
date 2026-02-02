#pragma once
#include "Export.h"
#include "Component.h"
#include "RenderShared.h"

namespace MMMEngine {
	class MMMENGINE_API Animator : public Component
	{
	private:
		ObjPtr<SkinRenderer> mSkinComp;

		Mesh_VecKey Evaluate(const Mesh_VecKey& _k1, const Mesh_VecKey& _k2, float _currTime);
		Mesh_QuatKey Evaluate(const Mesh_QuatKey& _k1, const Mesh_QuatKey& _k2, float _currTime);
	public:
		void Initialize() override;
		void UnInitialize() override;
	};
}


