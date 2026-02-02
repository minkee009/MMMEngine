#pragma once
#include "Renderer.h"
#include "Export.h"
#include "ResourceManager.h"
#include "rttr/type"
#include "RenderShared.h"

namespace MMMEngine {
	class SkeletalMesh;
	class Material;
	class Animator;
	class MMMENGINE_API SkinRenderer : public Renderer
	{
		RTTR_ENABLE(Renderer)
			RTTR_REGISTRATION_FRIEND
			friend class Animator;
	private:
		// GPU 버퍼
		ResPtr<SkeletalMesh> mesh = nullptr;
		Animator* mAnimator = nullptr;
		Mesh_BoneBuffer mAnimBuffer;

		void Initialize() override;
		void UnInitialize() override;
		void Render() override;
	public:
		ResPtr<SkeletalMesh>& GetMesh() { return mesh; }
		void SetMesh(ResPtr<SkeletalMesh>& _mesh);

		void SetCastShadow(bool _val);
		bool GetCastShadow();
		void SetReceiveShadow(bool _val);
		bool GetReceiveShadow();

		bool SetAnimatior(Animator* _animator);
		void RemoveAnimator() { mAnimator = nullptr; }

		//void SetMaterial(std::vector<ResPtr<Material>> _materials);
		//std::vector<ResPtr<Material>> GetMaterial();
	};
}


