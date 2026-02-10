#include "LineRenderer.h"

#include "Camera.h"
#include "Material.h"
#include "PShader.h"
#include "RenderCommand.h"
#include "RenderManager.h"
#include "RendererTools.h"
#include "ResourceManager.h"
#include "ShaderInfo.h"
#include "Transform.h"
#include "VShader.h"

#include "rttr/registration.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace
{
	constexpr float kLineEpsilon = 1.0e-6f;
	constexpr wchar_t kLineDefaultPShaderPath[] = L"Shader/PBR/PS/LineUnlitPS.hlsl";

	MMMEngine::ResPtr<MMMEngine::PShader> ResolveLineDefaultPShader()
	{
		auto lineUnlit = MMMEngine::ResourceManager::Get().Load<MMMEngine::PShader>(kLineDefaultPShaderPath);
		if (lineUnlit)
			return lineUnlit;

		return MMMEngine::ShaderInfo::Get().GetDefaultPShader();
	}

	struct SceneViewLineContext
	{
		bool active = false;
		DirectX::SimpleMath::Matrix viewMatrix = DirectX::SimpleMath::Matrix::Identity;
	};

	SceneViewLineContext g_sceneViewLineContext;
}

RTTR_REGISTRATION
{
	using namespace rttr;
	using namespace MMMEngine;

	registration::class_<LineRenderer>("LineRenderer")
		(rttr::metadata("wrapper_type_name", "ObjPtr<LineRenderer>"))
		.property("Material", &LineRenderer::GetMaterial, &LineRenderer::SetMaterial)
		.property("StartPoint", &LineRenderer::GetStartPoint, &LineRenderer::SetStartPoint)
		.property("EndPoint", &LineRenderer::GetEndPoint, &LineRenderer::SetEndPoint)
		.property("UseWorldSpace", &LineRenderer::GetUseWorldSpace, &LineRenderer::SetUseWorldSpace)
		.property("CameraFacing", &LineRenderer::GetUseCameraFacing, &LineRenderer::SetUseCameraFacing)
		.property("Width", &LineRenderer::GetWidth, &LineRenderer::SetWidth)(rttr::metadata("RANGE", "0.001,10.0"))
		.property("DashLength", &LineRenderer::GetDashLength, &LineRenderer::SetDashLength)(rttr::metadata("RANGE", "0.0,100.0"))
		.property("GapLength", &LineRenderer::GetGapLength, &LineRenderer::SetGapLength)(rttr::metadata("RANGE", "0.0,100.0"))
		.property("Color", &LineRenderer::GetColor, &LineRenderer::SetColor)
		.property("CastShadow", &LineRenderer::GetCastShadow, &LineRenderer::SetCastShadow)
		.property("ReceiveShadow", &LineRenderer::GetReceiveShadow, &LineRenderer::SetReceiveShadow);

	registration::class_<ObjPtr<LineRenderer>>("ObjPtr<LineRenderer>")
		.constructor<>(
			[]() {
				return Object::NewObject<LineRenderer>();
			})
		.method("Inject", &ObjPtr<LineRenderer>::Inject);
}

namespace MMMEngine
{
	void LineRenderer::BeginSceneViewRender(const DirectX::SimpleMath::Matrix& viewMatrix)
	{
		g_sceneViewLineContext.active = true;
		g_sceneViewLineContext.viewMatrix = viewMatrix;
	}

	void LineRenderer::EndSceneViewRender()
	{
		g_sceneViewLineContext.active = false;
		g_sceneViewLineContext.viewMatrix = DirectX::SimpleMath::Matrix::Identity;
	}

	void LineRenderer::SetMaterial(ResPtr<Material> material)
	{
		mMaterial = material;
		mIsAutoMaterial = false;
		if (!mMaterial)
			return;

		if (!mMaterial->GetVShader())
			mMaterial->SetVShader(ShaderInfo::Get().GetDefaultVShader());

		if (!mMaterial->GetPShader())
			mMaterial->SetPShader(ResolveLineDefaultPShader());

		auto pShader = mMaterial->GetPShader();
		if (pShader)
		{
			const ShaderType type = ShaderInfo::Get().GetShaderType(pShader->GetFilePath());
			ShaderInfo::Get().ConvertMaterialType(type, mMaterial.get());
		}

	}

	void LineRenderer::SetWidth(float width)
	{
		mWidth = std::max(width, 0.001f);
	}

	void LineRenderer::SetColor(const DirectX::SimpleMath::Color& color)
	{
		mColor = DirectX::SimpleMath::Color(
			std::clamp(color.x, 0.0f, 1.0f),
			std::clamp(color.y, 0.0f, 1.0f),
			std::clamp(color.z, 0.0f, 1.0f),
			std::clamp(color.w, 0.0f, 1.0f));

		if (mMaterial)
		{
			if (mIsAutoMaterial)
				ApplyColorToMaterial(mMaterial);
		}
	}

	void LineRenderer::Initialize()
	{
		renderIndex = RenderManager::Get().AddRenderer(this);
		EnsureMaterial();
	}

	void LineRenderer::UnInitialize()
	{
		RenderManager::Get().RemoveRenderer(renderIndex);
		mVertexBuffer.Reset();
		mIndexBuffer.Reset();
		mVertexCapacity = 0;
		mIndexCapacity = 0;
		mMaterial.reset();
		mIsAutoMaterial = false;
	}

	void LineRenderer::Render()
	{
		auto transform = GetTransform();
		if (!transform)
			return;

		EnsureMaterial();
		if (!mMaterial || !mMaterial->GetVShader() || !mMaterial->GetPShader())
			return;

		if (mIsAutoMaterial)
			ApplyColorToMaterial(mMaterial);

		DirectX::SimpleMath::Vector3 start = mStartPoint;
		DirectX::SimpleMath::Vector3 end = mEndPoint;
		if (!mUseWorldSpace)
		{
			const auto& world = transform->GetWorldMatrix();
			start = DirectX::SimpleMath::Vector3::Transform(start, world);
			end = DirectX::SimpleMath::Vector3::Transform(end, world);
		}

		DirectX::SimpleMath::Vector3 lineCenter = (start + end) * 0.5f;
		DirectX::SimpleMath::Vector3 cameraPos = lineCenter + DirectX::SimpleMath::Vector3::Backward;
		if (g_sceneViewLineContext.active)
		{
			DirectX::SimpleMath::Matrix invView = g_sceneViewLineContext.viewMatrix.Invert();
			cameraPos = invView.Translation();
		}
		else if (auto camera = RenderManager::Get().GetCamera(); camera.IsValid())
		{
			if (auto camTransform = camera->GetTransform(); camTransform.IsValid())
				cameraPos = camTransform->GetWorldPosition();
		}

		std::vector<Mesh_Vertex> vertices;
		std::vector<UINT> indices;
		if (!BuildLineGeometry(cameraPos, start, end, vertices, indices))
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
		command.camDistance = DirectX::SimpleMath::Vector3::Distance(cameraPos, lineCenter);

		// Line is rendered in the blended path to support material alpha.
		RenderManager::Get().AddCommand(RenderType::R_PARTICLE, std::move(command));
	}

	void LineRenderer::EnsureMaterial()
	{
		if (mMaterial)
			return;

		const auto defaultVS = ShaderInfo::Get().GetDefaultVShader();
		const auto defaultPS = ResolveLineDefaultPShader();
		if (!defaultVS || !defaultPS)
			return;

		auto material = std::make_shared<Material>();
		material->SetVShader(defaultVS);
		material->SetPShader(defaultPS);

		const ShaderType type = ShaderInfo::Get().GetShaderType(defaultPS->GetFilePath());
		ShaderInfo::Get().ConvertMaterialType(type, material.get());
		mMaterial = material;
		mIsAutoMaterial = true;

		ApplyColorToMaterial(mMaterial);
	}

	void LineRenderer::ApplyColorToMaterial(const ResPtr<Material>& material) const
	{
		if (!material)
			return;

		const DirectX::SimpleMath::Vector4 colorValue = {
			mColor.x, mColor.y, mColor.z, mColor.w
		};

		const auto& props = material->GetProperties();
		if (props.find(L"mBaseColor") != props.end())
			material->SetProperty(L"mBaseColor", colorValue);
		else
			material->AddProperty(L"mBaseColor", colorValue);
	}

	bool LineRenderer::BuildLineGeometry(
		const DirectX::SimpleMath::Vector3& cameraPosition,
		const DirectX::SimpleMath::Vector3& start,
		const DirectX::SimpleMath::Vector3& end,
		std::vector<Mesh_Vertex>& outVertices,
		std::vector<UINT>& outIndices) const
	{
		outVertices.clear();
		outIndices.clear();

		if (mWidth <= 0.0f)
			return false;

		const DirectX::SimpleMath::Vector3 lineVector = end - start;
		const float lineLength = lineVector.Length();
		if (lineLength < kLineEpsilon)
			return false;
		const DirectX::SimpleMath::Vector3 tangent = lineVector / lineLength;
		DirectX::SimpleMath::Vector3 fixedReference = DirectX::SimpleMath::Vector3::Up;
		if (std::abs(tangent.Dot(fixedReference)) > 0.999f)
			fixedReference = DirectX::SimpleMath::Vector3::Right;

		auto buildReferenceDir = [&](const DirectX::SimpleMath::Vector3& point) {
			if (!mUseCameraFacing)
				return fixedReference;

			DirectX::SimpleMath::Vector3 referenceDir = cameraPosition - point;
			if (referenceDir.LengthSquared() < kLineEpsilon)
				referenceDir = fixedReference;
			referenceDir.Normalize();
			return referenceDir;
		};

		auto buildSide = [&](const DirectX::SimpleMath::Vector3& viewDir) {
			DirectX::SimpleMath::Vector3 side = tangent.Cross(viewDir);
			if (side.LengthSquared() < kLineEpsilon)
				side = tangent.Cross(DirectX::SimpleMath::Vector3::Up);
			if (side.LengthSquared() < kLineEpsilon)
				side = tangent.Cross(DirectX::SimpleMath::Vector3::Right);
			if (side.LengthSquared() < kLineEpsilon)
				side = DirectX::SimpleMath::Vector3::Right;
			side.Normalize();
			return side;
		};

		const float halfWidth = mWidth * 0.5f;
		auto appendSegment = [&](float segmentStartDistance, float segmentEndDistance) {
			if (segmentEndDistance - segmentStartDistance <= kLineEpsilon)
				return;

			const DirectX::SimpleMath::Vector3 segmentStart = start + tangent * segmentStartDistance;
			const DirectX::SimpleMath::Vector3 segmentEnd = start + tangent * segmentEndDistance;
			const DirectX::SimpleMath::Vector3 viewDirStart = buildReferenceDir(segmentStart);
			const DirectX::SimpleMath::Vector3 viewDirEnd = buildReferenceDir(segmentEnd);

			DirectX::SimpleMath::Vector3 sideStart = buildSide(viewDirStart);
			DirectX::SimpleMath::Vector3 sideEnd = buildSide(viewDirEnd);
			if (sideStart.Dot(sideEnd) < 0.0f)
				sideEnd = -sideEnd;

			const DirectX::SimpleMath::Vector3 offsetStart = sideStart * halfWidth;
			const DirectX::SimpleMath::Vector3 offsetEnd = sideEnd * halfWidth;
			const float u0 = std::clamp(segmentStartDistance / lineLength, 0.0f, 1.0f);
			const float u1 = std::clamp(segmentEndDistance / lineLength, 0.0f, 1.0f);

			Mesh_Vertex v0{};
			v0.Pos = segmentStart - offsetStart;
			v0.Normal = viewDirStart;
			v0.Tangent = tangent;
			v0.UV = { u0, 0.0f };

			Mesh_Vertex v1 = v0;
			v1.Pos = segmentStart + offsetStart;
			v1.UV = { u0, 1.0f };

			Mesh_Vertex v2 = v0;
			v2.Pos = segmentEnd + offsetEnd;
			v2.Normal = viewDirEnd;
			v2.UV = { u1, 1.0f };

			Mesh_Vertex v3 = v0;
			v3.Pos = segmentEnd - offsetEnd;
			v3.Normal = viewDirEnd;
			v3.UV = { u1, 0.0f };

			const UINT baseIndex = static_cast<UINT>(outVertices.size());
			outVertices.push_back(v0);
			outVertices.push_back(v1);
			outVertices.push_back(v2);
			outVertices.push_back(v3);

			outIndices.push_back(baseIndex + 0);
			outIndices.push_back(baseIndex + 1);
			outIndices.push_back(baseIndex + 2);
			outIndices.push_back(baseIndex + 0);
			outIndices.push_back(baseIndex + 2);
			outIndices.push_back(baseIndex + 3);
		};

		const bool useDashedPattern = (mDashLength > kLineEpsilon) && (mGapLength > kLineEpsilon);
		if (useDashedPattern)
		{
			const float cycleLength = mDashLength + mGapLength;
			const size_t estimatedSegmentCount = static_cast<size_t>(std::ceil(lineLength / cycleLength));
			outVertices.reserve(estimatedSegmentCount * 4);
			outIndices.reserve(estimatedSegmentCount * 6);

			float distance = 0.0f;
			while (distance < lineLength)
			{
				const float dashStartDistance = distance;
				const float dashEndDistance = std::min(distance + mDashLength, lineLength);
				appendSegment(dashStartDistance, dashEndDistance);
				distance += cycleLength;
			}
		}
		else
		{
			outVertices.reserve(4);
			outIndices.reserve(6);
			appendSegment(0.0f, lineLength);
		}

		return !outVertices.empty() && !outIndices.empty();
	}

	bool LineRenderer::UpdateGpuBuffers(
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
