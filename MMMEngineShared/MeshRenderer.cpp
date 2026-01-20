#include "MeshRenderer.h"
#include "RendererBase.h"
#include "StaticMesh.h"
#include <RenderManager.h>
#include <GeoRenderer.h>
#include "Object.h"
#include "Transform.h"

MMMEngine::MeshRenderer::MeshRenderer()
{
	REGISTER_BEHAVIOUR_MESSAGE(Start);
	REGISTER_BEHAVIOUR_MESSAGE(Update);
}

void MMMEngine::MeshRenderer::SetMesh(ResPtr<StaticMesh>& _mesh)
{
	mesh = _mesh;
	Start();
}

void MMMEngine::MeshRenderer::Start()
{
	// 유효성 확인
	if (!mesh || !mesh->meshData || !mesh->gpuBuffer)
		return;

	for (auto& [matIdx, meshIndices] : mesh->meshGroupData) {
		auto& material = mesh->meshData->materials[matIdx];

		for (const auto& idx : meshIndices) {
			std::weak_ptr<RendererBase> renderer;

			renderer = RenderManager::Get().AddRenderer<GeoRenderer>(RenderType::GEOMETRY);

			auto& meshBuffer = mesh->gpuBuffer->vertexBuffers[idx];
			auto& indicesBuffer = mesh->gpuBuffer->indexBuffers[idx];
			UINT indicesSize = static_cast<UINT>(mesh->meshData->indices[idx].size());

			if (auto locked = renderer.lock()) {
				locked->SetRenderData(meshBuffer, indicesBuffer, indicesSize, material);
				renderers.push_back(renderer);
			}
		}
	}
}

void MMMEngine::MeshRenderer::Update()
{
	// 월드 매트릭스 전달
	for (auto& renderer : renderers) {
		if (auto locked = renderer.lock()) {
			if (auto transform = GetGameObject()->GetTransform())
				locked->SetWorldMat(transform->GetWorldMatrix());
		}
	}
}
