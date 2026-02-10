#pragma once

#include "Export.h"
#include "Renderer.h"
#include "RenderShared.h"
#include "ResourceManager.h"
#include "rttr/type"

#include <SimpleMath.h>
#include <d3d11_4.h>
#include <wrl/client.h>
#include <algorithm>
#include <vector>

namespace MMMEngine
{
	class Material;

	class MMMENGINE_API LineRenderer : public Renderer
	{
		RTTR_ENABLE(Renderer)
		RTTR_REGISTRATION_FRIEND

	private:
		ResPtr<Material> mMaterial = nullptr;
		bool mIsAutoMaterial = false;

		Microsoft::WRL::ComPtr<ID3D11Buffer> mVertexBuffer;
		Microsoft::WRL::ComPtr<ID3D11Buffer> mIndexBuffer;
		size_t mVertexCapacity = 0;
		size_t mIndexCapacity = 0;

		DirectX::SimpleMath::Vector3 mStartPoint = { -0.5f, 0.0f, 0.0f };
		DirectX::SimpleMath::Vector3 mEndPoint = { 0.5f, 0.0f, 0.0f };
		float mWidth = 0.1f;
		float mDashLength = 0.0f;
		float mGapLength = 0.0f;
		DirectX::SimpleMath::Color mColor = { 1.0f, 1.0f, 1.0f, 1.0f };
		bool mUseWorldSpace = false;
		bool mUseCameraFacing = true;

		void Initialize() override;
		void UnInitialize() override;
		void Render() override;

		void EnsureMaterial();
		void ApplyColorToMaterial(const ResPtr<Material>& material) const;
		bool BuildLineGeometry(
			const DirectX::SimpleMath::Vector3& cameraPosition,
			const DirectX::SimpleMath::Vector3& start,
			const DirectX::SimpleMath::Vector3& end,
			std::vector<Mesh_Vertex>& outVertices,
			std::vector<UINT>& outIndices) const;
		bool UpdateGpuBuffers(
			const std::vector<Mesh_Vertex>& vertices,
			const std::vector<UINT>& indices);

	public:
		static void BeginSceneViewRender(const DirectX::SimpleMath::Matrix& viewMatrix);
		static void EndSceneViewRender();

		void SetMaterial(ResPtr<Material> material);
		ResPtr<Material> GetMaterial() const { return mMaterial; }

		void SetStartPoint(const DirectX::SimpleMath::Vector3& startPoint) { mStartPoint = startPoint; }
		const DirectX::SimpleMath::Vector3& GetStartPoint() const { return mStartPoint; }

		void SetEndPoint(const DirectX::SimpleMath::Vector3& endPoint) { mEndPoint = endPoint; }
		const DirectX::SimpleMath::Vector3& GetEndPoint() const { return mEndPoint; }

		void SetWidth(float width);
		float GetWidth() const { return mWidth; }

		void SetDashLength(float dashLength) { mDashLength = std::max(dashLength, 0.0f); }
		float GetDashLength() const { return mDashLength; }

		void SetGapLength(float gapLength) { mGapLength = std::max(gapLength, 0.0f); }
		float GetGapLength() const { return mGapLength; }

		void SetColor(const DirectX::SimpleMath::Color& color);
		const DirectX::SimpleMath::Color& GetColor() const { return mColor; }

		void SetUseWorldSpace(bool useWorldSpace) { mUseWorldSpace = useWorldSpace; }
		bool GetUseWorldSpace() const { return mUseWorldSpace; }

		void SetUseCameraFacing(bool useCameraFacing) { mUseCameraFacing = useCameraFacing; }
		bool GetUseCameraFacing() const { return mUseCameraFacing; }

		void SetCastShadow(bool value) { castShadows = value; }
		bool GetCastShadow() const { return castShadows; }

		void SetReceiveShadow(bool value) { receiveShadows = value; }
		bool GetReceiveShadow() const { return receiveShadows; }
	};
}
