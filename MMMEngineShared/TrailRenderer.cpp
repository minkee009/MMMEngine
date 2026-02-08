#include "TrailRenderer.h"

#include "Camera.h"
#include "Material.h"
#include "PShader.h"
#include "RenderCommand.h"
#include "RenderManager.h"
#include "RendererTools.h"
#include "ResourceManager.h"
#include "ShaderInfo.h"
#include "TimeManager.h"
#include "Transform.h"
#include "VShader.h"

#include "rttr/registration.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace
{
	constexpr float kTrailEpsilon = 1.0e-6f;
	constexpr wchar_t kTrailDefaultPShaderPath[] = L"Shader/PBR/PS/TrailUnlitPS.hlsl";

	MMMEngine::ResPtr<MMMEngine::PShader> ResolveTrailDefaultPShader()
	{
		auto trailUnlit = MMMEngine::ResourceManager::Get().Load<MMMEngine::PShader>(kTrailDefaultPShaderPath);
		if (trailUnlit)
			return trailUnlit;

		return MMMEngine::ShaderInfo::Get().GetDefaultPShader();
	}
}

RTTR_REGISTRATION
{
	using namespace rttr;
	using namespace MMMEngine;

	registration::class_<TrailRenderer>("TrailRenderer")
		(rttr::metadata("wrapper_type_name", "ObjPtr<TrailRenderer>"))
		.property("Material", &TrailRenderer::GetMaterial, &TrailRenderer::SetMaterial)
		.property("Width", &TrailRenderer::GetWidth, &TrailRenderer::SetWidth)(rttr::metadata("RANGE", "0.01,10.0"))
		.property("TailWidthScale", &TrailRenderer::GetTailWidthScale, &TrailRenderer::SetTailWidthScale)(rttr::metadata("RANGE", "0.0,1.0"))
		.property("WidthTaperPower", &TrailRenderer::GetWidthTaperPower, &TrailRenderer::SetWidthTaperPower)(rttr::metadata("RANGE", "0.1,8.0"))
		.property("Time", &TrailRenderer::GetTime, &TrailRenderer::SetTime)(rttr::metadata("RANGE", "0.01,20.0"))
		.property("MinVertexDistance", &TrailRenderer::GetMinVertexDistance, &TrailRenderer::SetMinVertexDistance)(rttr::metadata("RANGE", "0.001,5.0"))
		.property("MaxPoints", &TrailRenderer::GetMaxPoints, &TrailRenderer::SetMaxPoints)(rttr::metadata("RANGE", "8,4096"))
		.property("Emitting", &TrailRenderer::GetEmitting, &TrailRenderer::SetEmitting)
		.property("CastShadow", &TrailRenderer::GetCastShadow, &TrailRenderer::SetCastShadow)
		.property("ReceiveShadow", &TrailRenderer::GetReceiveShadow, &TrailRenderer::SetReceiveShadow);

	registration::class_<ObjPtr<TrailRenderer>>("ObjPtr<TrailRenderer>")
		.constructor<>(
			[]() {
				return Object::NewObject<TrailRenderer>();
			})
		.method("Inject", &ObjPtr<TrailRenderer>::Inject);
}

namespace MMMEngine
{
	void TrailRenderer::SetMaterial(ResPtr<Material> material)
	{
		mMaterial = material;
		if (!mMaterial)
			return;

		if (!mMaterial->GetVShader())
			mMaterial->SetVShader(ShaderInfo::Get().GetDefaultVShader());

		if (!mMaterial->GetPShader())
			mMaterial->SetPShader(ResolveTrailDefaultPShader());

		auto pShader = mMaterial->GetPShader();
		if (pShader)
		{
			const ShaderType type = ShaderInfo::Get().GetShaderType(pShader->GetFilePath());
			ShaderInfo::Get().ConvertMaterialType(type, mMaterial.get());
		}
	}

	void TrailRenderer::SetWidth(float width)
	{
		mWidth = std::max(width, 0.001f);
	}

	void TrailRenderer::SetTailWidthScale(float scale)
	{
		mTailWidthScale = std::clamp(scale, 0.0f, 1.0f);
	}

	void TrailRenderer::SetWidthTaperPower(float power)
	{
		mWidthTaperPower = std::clamp(power, 0.1f, 8.0f);
	}

	void TrailRenderer::SetTime(float lifeTime)
	{
		mLifeTime = std::max(lifeTime, 0.01f);
	}

	void TrailRenderer::SetMinVertexDistance(float distance)
	{
		mMinVertexDistance = std::max(distance, 0.001f);
	}

	void TrailRenderer::SetMaxPoints(uint32_t maxPoints)
	{
		mMaxPoints = std::clamp(maxPoints, 8u, 4096u);
		while (mPoints.size() > static_cast<size_t>(mMaxPoints))
			mPoints.pop_front();
	}

	void TrailRenderer::Clear()
	{
		mPoints.clear();
	}

	void TrailRenderer::Initialize()
	{
		renderIndex = RenderManager::Get().AddRenderer(this);
		EnsureMaterial();
	}

	void TrailRenderer::UnInitialize()
	{
		RenderManager::Get().RemoveRenderer(renderIndex);
		mVertexBuffer.Reset();
		mIndexBuffer.Reset();
		mVertexCapacity = 0;
		mIndexCapacity = 0;
		mPoints.clear();
		mMaterial.reset();
	}

	void TrailRenderer::Render()
	{
		auto transform = GetTransform();
		if (!transform)
			return;

		EnsureMaterial();
		if (!mMaterial || !mMaterial->GetVShader() || !mMaterial->GetPShader())
			return;

		const float dt = TimeManager::Get().GetDeltaTime();
		const DirectX::SimpleMath::Vector3 emitterPos = transform->GetWorldPosition();
		UpdateTrailPoints(dt, emitterPos);

		if (mPoints.empty())
			return;

		DirectX::SimpleMath::Vector3 cameraPos = emitterPos + DirectX::SimpleMath::Vector3::Backward;
		if (auto camera = RenderManager::Get().GetCamera(); camera.IsValid())
		{
			if (auto camTransform = camera->GetTransform(); camTransform.IsValid())
				cameraPos = camTransform->GetWorldPosition();
		}

		std::vector<Mesh_Vertex> vertices;
		std::vector<UINT> indices;
		if (!BuildTrailGeometry(cameraPos, emitterPos, mEmitting, vertices, indices))
			return;

		if (!UpdateGpuBuffers(vertices, indices))
			return;

		RenderCommand command;
		command.vertexBuffer = mVertexBuffer.Get();
		command.indexBuffer = mIndexBuffer.Get();
		command.material = mMaterial;
		command.worldMatIndex = RenderManager::Get().AddMatrix(DirectX::SimpleMath::Matrix::Identity);
		command.indiciesSize = static_cast<UINT>(indices.size());
		command.rendererID = renderIndex;
		command.castShadow = castShadows;
		command.receiveShadow = receiveShadows;
		command.camDistance = DirectX::SimpleMath::Vector3::Distance(cameraPos, emitterPos);

		const std::wstring shaderPath = mMaterial->GetPShader()->GetFilePath();
		const RenderType type = ShaderInfo::Get().GetRenderType(shaderPath);
		RenderManager::Get().AddCommand(type, std::move(command));
	}

	void TrailRenderer::EnsureMaterial()
	{
		if (mMaterial)
			return;

		const auto defaultVS = ShaderInfo::Get().GetDefaultVShader();
		const auto defaultPS = ResolveTrailDefaultPShader();
		if (!defaultVS || !defaultPS)
			return;

		auto material = std::make_shared<Material>();
		material->SetVShader(defaultVS);
		material->SetPShader(defaultPS);

		const ShaderType type = ShaderInfo::Get().GetShaderType(defaultPS->GetFilePath());
		ShaderInfo::Get().ConvertMaterialType(type, material.get());
		mMaterial = material;
	}

	void TrailRenderer::UpdateTrailPoints(float dt, const DirectX::SimpleMath::Vector3& emitterPosition)
	{
		if (dt < 0.0f)
			dt = 0.0f;

		for (auto& point : mPoints)
			point.age += dt;

		while (!mPoints.empty() && mPoints.front().age > mLifeTime)
			mPoints.pop_front();

		if (!mEmitting)
			return;

		if (mPoints.empty())
		{
			mPoints.push_back({ emitterPosition, 0.0f });
			return;
		}

		const auto& head = mPoints.back();
		const float minDistSq = mMinVertexDistance * mMinVertexDistance;
		const float distSq = DirectX::SimpleMath::Vector3::DistanceSquared(head.position, emitterPosition);

		if (distSq >= minDistSq)
		{
			mPoints.push_back({ emitterPosition, 0.0f });
		}

		while (mPoints.size() > static_cast<size_t>(mMaxPoints))
			mPoints.pop_front();
	}

	bool TrailRenderer::BuildTrailGeometry(
		const DirectX::SimpleMath::Vector3& cameraPosition,
		const DirectX::SimpleMath::Vector3& emitterPosition,
		bool includeEmitterHead,
		std::vector<Mesh_Vertex>& outVertices,
		std::vector<UINT>& outIndices) const
	{
		outVertices.clear();
		outIndices.clear();

		std::vector<DirectX::SimpleMath::Vector3> trailPositions;
		trailPositions.reserve(mPoints.size() + 1);
		for (const auto& point : mPoints)
			trailPositions.push_back(point.position);

		if (includeEmitterHead)
		{
			if (trailPositions.empty())
			{
				trailPositions.push_back(emitterPosition);
			}
			else
			{
				const float headDistSq =
					DirectX::SimpleMath::Vector3::DistanceSquared(trailPositions.back(), emitterPosition);
				if (headDistSq > kTrailEpsilon)
					trailPositions.push_back(emitterPosition);
			}
		}

		const size_t pointCount = trailPositions.size();
		if (pointCount < 2 || mWidth <= 0.0f)
			return false;

		outVertices.reserve(pointCount * 2);
		outIndices.reserve((pointCount - 1) * 6);

		for (size_t i = 0; i < pointCount; ++i)
		{
			const auto& current = trailPositions[i];
			const auto& prev = (i > 0) ? trailPositions[i - 1] : current;
			const auto& next = (i + 1 < pointCount) ? trailPositions[i + 1] : current;

			DirectX::SimpleMath::Vector3 tangent = next - prev;
			if (tangent.LengthSquared() < kTrailEpsilon)
				tangent = (i > 0) ? (current - trailPositions[i - 1]) : DirectX::SimpleMath::Vector3::Forward;
			if (tangent.LengthSquared() < kTrailEpsilon)
				tangent = DirectX::SimpleMath::Vector3::Forward;
			tangent.Normalize();

			DirectX::SimpleMath::Vector3 viewDir = cameraPosition - current;
			if (viewDir.LengthSquared() < kTrailEpsilon)
				viewDir = DirectX::SimpleMath::Vector3::Up;
			viewDir.Normalize();

			DirectX::SimpleMath::Vector3 side = tangent.Cross(viewDir);
			if (side.LengthSquared() < kTrailEpsilon)
				side = tangent.Cross(DirectX::SimpleMath::Vector3::Up);
			if (side.LengthSquared() < kTrailEpsilon)
				side = tangent.Cross(DirectX::SimpleMath::Vector3::Right);
			if (side.LengthSquared() < kTrailEpsilon)
				side = DirectX::SimpleMath::Vector3::Right;
			side.Normalize();

			const float u = (pointCount > 1) ? static_cast<float>(i) / static_cast<float>(pointCount - 1) : 0.0f;
			const float taperT = std::pow(std::clamp(u, 0.0f, 1.0f), mWidthTaperPower);
			const float widthScale = mTailWidthScale + (1.0f - mTailWidthScale) * taperT;
			const float halfWidth = mWidth * 0.5f * widthScale;

			Mesh_Vertex left{};
			left.Pos = current - side * halfWidth;
			left.Normal = viewDir;
			left.Tangent = tangent;
			left.UV = { u, 0.0f };

			Mesh_Vertex right = left;
			right.Pos = current + side * halfWidth;
			right.UV = { u, 1.0f };

			outVertices.push_back(left);
			outVertices.push_back(right);

			if (i + 1 < pointCount)
			{
				const UINT base = static_cast<UINT>(i * 2);

				outIndices.push_back(base);
				outIndices.push_back(base + 1);
				outIndices.push_back(base + 2);

				outIndices.push_back(base + 1);
				outIndices.push_back(base + 3);
				outIndices.push_back(base + 2);
			}
		}

		return !outVertices.empty() && !outIndices.empty();
	}

	bool TrailRenderer::UpdateGpuBuffers(
		const std::vector<Mesh_Vertex>& vertices,
		const std::vector<UINT>& indices)
	{
		auto device = RenderManager::Get().GetDevice();
		auto context = RenderManager::Get().GetContext();
		if (!device || !context)
			return false;
		if (vertices.empty() || indices.empty())
			return false;

		if (!mVertexBuffer || mVertexCapacity < vertices.size())
		{
			D3D11_BUFFER_DESC vbDesc = {};
			vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
			vbDesc.Usage = D3D11_USAGE_DYNAMIC;
			vbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
			vbDesc.ByteWidth = static_cast<UINT>(sizeof(Mesh_Vertex) * vertices.size());

			Microsoft::WRL::ComPtr<ID3D11Buffer> newBuffer;
			HR_T(device->CreateBuffer(&vbDesc, nullptr, newBuffer.GetAddressOf()));
			if (!newBuffer)
				return false;

			mVertexBuffer = std::move(newBuffer);
			mVertexCapacity = vertices.size();
		}

		if (!mIndexBuffer || mIndexCapacity < indices.size())
		{
			D3D11_BUFFER_DESC ibDesc = {};
			ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
			ibDesc.Usage = D3D11_USAGE_DYNAMIC;
			ibDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
			ibDesc.ByteWidth = static_cast<UINT>(sizeof(UINT) * indices.size());

			Microsoft::WRL::ComPtr<ID3D11Buffer> newBuffer;
			HR_T(device->CreateBuffer(&ibDesc, nullptr, newBuffer.GetAddressOf()));
			if (!newBuffer)
				return false;

			mIndexBuffer = std::move(newBuffer);
			mIndexCapacity = indices.size();
		}

		D3D11_MAPPED_SUBRESOURCE mapped = {};
		if (FAILED(context->Map(mVertexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
			return false;
		std::memcpy(mapped.pData, vertices.data(), sizeof(Mesh_Vertex) * vertices.size());
		context->Unmap(mVertexBuffer.Get(), 0);

		if (FAILED(context->Map(mIndexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
			return false;
		std::memcpy(mapped.pData, indices.data(), sizeof(UINT) * indices.size());
		context->Unmap(mIndexBuffer.Get(), 0);

		return true;
	}
}
