#pragma once

#include "Export.h"
#include "Renderer.h"
#include "RenderShared.h"
#include "ResourceManager.h"
#include "rttr/type"

#include <deque>
#include <vector>
#include <wrl/client.h>

namespace MMMEngine
{
	class Material;
	class MMMENGINE_API TrailRenderer : public Renderer
	{
		RTTR_ENABLE(Renderer)
		RTTR_REGISTRATION_FRIEND
	private:
		struct TrailPoint
		{
			DirectX::SimpleMath::Vector3 position = DirectX::SimpleMath::Vector3::Zero;
			float age = 0.0f;
		};

		ResPtr<Material> mMaterial = nullptr;
		std::deque<TrailPoint> mPoints;

		Microsoft::WRL::ComPtr<ID3D11Buffer> mVertexBuffer;
		Microsoft::WRL::ComPtr<ID3D11Buffer> mIndexBuffer;
		size_t mVertexCapacity = 0;
		size_t mIndexCapacity = 0;

		float mWidth = 0.25f;
		float mTailWidthScale = 1.0f;
		float mWidthTaperPower = 1.0f;
		float mLifeTime = 0.6f;
		float mMinVertexDistance = 0.1f;
		uint32_t mMaxPoints = 128;
		bool mEmitting = true;

		void Initialize() override;
		void UnInitialize() override;
		void Render() override;

		void EnsureMaterial();
		void UpdateTrailPoints(float dt, const DirectX::SimpleMath::Vector3& emitterPosition);
		bool BuildTrailGeometry(
			const DirectX::SimpleMath::Vector3& cameraPosition,
			const DirectX::SimpleMath::Vector3& emitterPosition,
			bool includeEmitterHead,
			std::vector<Mesh_Vertex>& outVertices,
			std::vector<UINT>& outIndices) const;
		bool UpdateGpuBuffers(
			const std::vector<Mesh_Vertex>& vertices,
			const std::vector<UINT>& indices);
	public:
		void SetMaterial(ResPtr<Material> material);
		ResPtr<Material> GetMaterial() const { return mMaterial; }

		void SetWidth(float width);
		float GetWidth() const { return mWidth; }

		void SetTailWidthScale(float scale);
		float GetTailWidthScale() const { return mTailWidthScale; }

		void SetWidthTaperPower(float power);
		float GetWidthTaperPower() const { return mWidthTaperPower; }

		void SetTime(float lifeTime);
		float GetTime() const { return mLifeTime; }

		void SetMinVertexDistance(float distance);
		float GetMinVertexDistance() const { return mMinVertexDistance; }

		void SetMaxPoints(uint32_t maxPoints);
		uint32_t GetMaxPoints() const { return mMaxPoints; }

		void SetEmitting(bool emitting) { mEmitting = emitting; }
		bool GetEmitting() const { return mEmitting; }

		void SetCastShadow(bool value) { castShadows = value; }
		bool GetCastShadow() const { return castShadows; }

		void SetReceiveShadow(bool value) { receiveShadows = value; }
		bool GetReceiveShadow() const { return receiveShadows; }

		void Clear();
	};
}
