#pragma once

#include "Export.h"
#include "ResourceManager.h"
#include "Renderer.h"
#include "rttr/type"
#include "SimpleMath.h"

namespace MMMEngine {
	class StaticMesh;
	class Material;
	class MMMENGINE_API MeshRenderer : public Renderer {
		RTTR_ENABLE(Renderer)
		RTTR_REGISTRATION_FRIEND
	private:
		// GPU 버퍼
		ResPtr<StaticMesh> mesh = nullptr;
		bool m_boundsDirty = true;
		DirectX::SimpleMath::Vector3 m_localBoundsCenter = DirectX::SimpleMath::Vector3::Zero;
		float m_localBoundsRadius = -1.0f;

		void RebuildLocalBounds();
		bool IsVisibleInCurrentView();

		void Initialize() override;
		void UnInitialize() override;
		void Render() override;
	public:
		ResPtr<StaticMesh> GetMesh() { return mesh; }
		void SetMesh(ResPtr<StaticMesh> _mesh);

		void SetCastShadow(bool _val);
		bool GetCastShadow();
		void SetReceiveShadow(bool _val);
		bool GetReceiveShadow();

		void SetMaterial(std::vector<ResPtr<Material>>& _materials);
		std::vector<ResPtr<Material>>& GetMaterial();
	};
}

