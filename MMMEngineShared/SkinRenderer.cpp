#include "pch.h"
#include "SkinRenderer.h"

#include "RenderManager.h"
#include "RenderCommand.h"
#include "GameObject.h"
#include "Transform.h"
#include "ShaderInfo.h"
#include "PShader.h"
#include "Material.h"
#include "Animator.h"
#include "TimeManager.h"

#include "SkeletalMesh.h"
#include "rttr/registration.h"

RTTR_REGISTRATION
{
	using namespace rttr;
	using namespace MMMEngine;

	registration::class_<SkinRenderer>("SkinRenderer")
		(rttr::metadata("wrapper_type_name", "ObjPtr<SkinRenderer>"))
		.property("Mesh", &SkinRenderer::GetMesh, &SkinRenderer::SetMesh)
		//.property("Materials", &SkinRenderer::GetMaterial, &SkinRenderer::SetMaterial)
		.property("CastShadow", &SkinRenderer::GetCastShadow, &SkinRenderer::SetCastShadow)
		.property("ReceiveShadow", &SkinRenderer::GetReceiveShadow, &SkinRenderer::SetReceiveShadow);

	registration::class_<ObjPtr<SkinRenderer>>("ObjPtr<SkinRenderer>")
		.constructor<>(
			[]() {
				return Object::NewObject<SkinRenderer>();
			})
		.method("Inject", &ObjPtr<SkinRenderer>::Inject);
}

namespace MMMEngine {
	void SkinRenderer::SetMesh(ResPtr<SkeletalMesh>& _mesh)
	{
		mesh = _mesh;
	}

	bool SkinRenderer::GetCastShadow()
	{
		if (!mesh)
			return false;

		return castShadows;
	}

	void SkinRenderer::SetCastShadow(bool _val)
	{
		if (!mesh)
			return;

		castShadows = _val;
	}

	void SkinRenderer::SetReceiveShadow(bool _val)
	{
		if (!mesh)
			return;

		receiveShadows = _val;
	}

	bool SkinRenderer::GetReceiveShadow()
	{
		if (!mesh)
			return false;

		return receiveShadows;
	}

	bool SkinRenderer::SetAnimatior(Animator* _animator)
	{
		if (mAnimator != nullptr)
			return false;

		mAnimator = _animator;
		return true;
	}

	void SkinRenderer::Initialize()
	{
		renderIndex = RenderManager::Get().AddRenderer(this);
	}

	void SkinRenderer::UnInitialize()
	{
		RenderManager::Get().RemoveRenderer(renderIndex);
	}

	void SkinRenderer::Render()
	{
		// 유효성 확인
		if (!mesh || !GetTransform())
			return;

		if (mAnimator != nullptr)
			mAnimator->Update(TimeManager::Get().GetDeltaTime());

		for (auto& [matIdx, meshIndices] : mesh->meshGroupData) {
			if (mesh->materials.empty())
				continue;

			auto& material = mesh->materials[matIdx];

			if (!material)
				continue;

			for (const auto& idx : meshIndices) {
				RenderCommand command;
				auto& meshBuffer = mesh->gpuBuffer.vertexBuffers[idx];
				auto& indicesBuffer = mesh->gpuBuffer.indexBuffers[idx];

				command.vertexBuffer = meshBuffer.Get();
				command.indexBuffer = indicesBuffer.Get();
				command.material = material;
				command.worldMatIndex = RenderManager::Get().AddMatrix(GetTransform()->GetWorldMatrix());
				command.indiciesSize = mesh->indexSizes[idx];
				command.rendererID = renderIndex;
				command.castShadow = castShadows;
				command.receiveShadow = receiveShadows;

				if (mAnimBuffer != nullptr) {
					command.offsetBuffer = &mesh->offsetBuffer;
					command.animBuffer = mAnimBuffer;
				}
				else {
					command.offsetBuffer = &mDefaultBoneBuffer;
					command.animBuffer = &mDefaultBoneBuffer;
				}
				

				// TODO::TransCulant일시 CamDistance 보내줘야함!!
				command.camDistance = 0.0f;

				std::wstring shaderPath = material->GetPShader()->GetFilePath();
				RenderType type = ShaderInfo::Get().GetRenderType(shaderPath);

				RenderManager::Get().AddCommand(type, std::move(command));
			}
		}
	}
}
