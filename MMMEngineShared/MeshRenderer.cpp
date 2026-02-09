#include "MeshRenderer.h"
#include "RenderManager.h"
#include "RenderCommand.h"
#include "GameObject.h"
#include "Transform.h"
#include "ShaderInfo.h"
#include "PShader.h"
#include "Material.h"

#include "StaticMesh.h"
#include "rttr/registration.h"
#include "SkinRenderer.h"
#include <cfloat>
#include <cmath>
#include <algorithm>

RTTR_REGISTRATION
{
	using namespace rttr;
	using namespace MMMEngine;

	registration::class_<MeshRenderer>("MeshRenderer")
		(rttr::metadata("wrapper_type_name", "ObjPtr<MeshRenderer>"))
		.property("Mesh", &MeshRenderer::GetMesh, &MeshRenderer::SetMesh)
		//.property("Materials", &MeshRenderer::GetMaterial, &MeshRenderer::SetMaterial)
		.property("CastShadow", &MeshRenderer::GetCastShadow, &MeshRenderer::SetCastShadow)
		.property("ReceiveShadow", &MeshRenderer::GetReceiveShadow, &MeshRenderer::SetReceiveShadow);

	registration::class_<ObjPtr<MeshRenderer>>("ObjPtr<MeshRenderer>")
		.constructor<>(
			[]() {
				return Object::NewObject<MeshRenderer>();
			})
		.method("Inject", &ObjPtr<MeshRenderer>::Inject);
}

void MMMEngine::MeshRenderer::SetMesh(ResPtr<StaticMesh> _mesh)
{
	mesh = _mesh;
	m_boundsDirty = true;
}
 
bool MMMEngine::MeshRenderer::GetCastShadow()
{
	if (!mesh)
		return false;

	return castShadows;
}

void MMMEngine::MeshRenderer::SetCastShadow(bool _val)
{
	if (!mesh)
		return;

	castShadows = _val;
}

void MMMEngine::MeshRenderer::SetReceiveShadow(bool _val)
{
	if (!mesh)
		return;

	receiveShadows = _val;
}

bool MMMEngine::MeshRenderer::GetReceiveShadow()
{
	if (!mesh)
		return false;
	
	return receiveShadows;
}

std::vector<MMMEngine::ResPtr<MMMEngine::Material>>& MMMEngine::MeshRenderer::GetMaterial()
{
	return mesh->materials;
}

void MMMEngine::MeshRenderer::SetMaterial(std::vector<ResPtr<Material>>& _materials)
{
	if (!mesh)
		return;
	if (mesh->materials.size() != _materials.size())
		return;
	
	mesh->materials = _materials;
}

void MMMEngine::MeshRenderer::Initialize()
{
	renderIndex = RenderManager::Get().AddRenderer(this);
}

void MMMEngine::MeshRenderer::UnInitialize()
{
	RenderManager::Get().RemoveRenderer(renderIndex);
	mesh.reset();
	m_boundsDirty = true;
	m_localBoundsRadius = -1.0f;
}

void MMMEngine::MeshRenderer::RebuildLocalBounds()
{
	m_boundsDirty = false;
	m_localBoundsCenter = DirectX::SimpleMath::Vector3::Zero;
	m_localBoundsRadius = -1.0f;

	if (!mesh)
		return;

	DirectX::SimpleMath::Vector3 minPos(FLT_MAX, FLT_MAX, FLT_MAX);
	DirectX::SimpleMath::Vector3 maxPos(-FLT_MAX, -FLT_MAX, -FLT_MAX);
	bool hasVertex = false;

	for (const auto& submesh : mesh->meshData.vertices)
	{
		for (const auto& v : submesh)
		{
			hasVertex = true;
			minPos.x = std::min(minPos.x, v.Pos.x);
			minPos.y = std::min(minPos.y, v.Pos.y);
			minPos.z = std::min(minPos.z, v.Pos.z);
			maxPos.x = std::max(maxPos.x, v.Pos.x);
			maxPos.y = std::max(maxPos.y, v.Pos.y);
			maxPos.z = std::max(maxPos.z, v.Pos.z);
		}
	}

	if (!hasVertex)
		return;

	m_localBoundsCenter = (minPos + maxPos) * 0.5f;

	float maxDistSq = 0.0f;
	for (const auto& submesh : mesh->meshData.vertices)
	{
		for (const auto& v : submesh)
		{
			const auto diff = v.Pos - m_localBoundsCenter;
			maxDistSq = std::max(maxDistSq, diff.LengthSquared());
		}
	}

	m_localBoundsRadius = std::sqrt(maxDistSq);
	if (m_localBoundsRadius < 0.001f)
		m_localBoundsRadius = 0.001f;
}

bool MMMEngine::MeshRenderer::IsVisibleInCurrentView()
{
	if (!mesh || !GetTransform())
		return false;

	if (m_boundsDirty)
		RebuildLocalBounds();

	if (m_localBoundsRadius <= 0.0f)
		return true;

	const auto world = GetTransform()->GetWorldMatrix();
	const auto worldCenter = DirectX::SimpleMath::Vector3::Transform(m_localBoundsCenter, world);

	const DirectX::SimpleMath::Vector3 axisX(world._11, world._12, world._13);
	const DirectX::SimpleMath::Vector3 axisY(world._21, world._22, world._23);
	const DirectX::SimpleMath::Vector3 axisZ(world._31, world._32, world._33);

	const float maxScale = std::max({ axisX.Length(), axisY.Length(), axisZ.Length() });
	const float worldRadius = m_localBoundsRadius * std::max(maxScale, 1.0f) * 1.05f;

	return RenderManager::Get().IsSphereVisible(worldCenter, worldRadius);
}

void MMMEngine::MeshRenderer::Render()
{
	// 유효성 확인
	if (!mesh || !GetTransform())
		return;

	if (!IsVisibleInCurrentView())
		return;

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

			// TODO::CamDistance 보내줘야함!!
			command.camDistance = 0.0f;

			std::wstring shaderPath = material->GetPShader()->GetFilePath();
			RenderType type = ShaderInfo::Get().GetRenderType(shaderPath);

			RenderManager::Get().AddCommand(type, std::move(command));
		}
	}
}
