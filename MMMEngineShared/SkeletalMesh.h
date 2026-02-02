#pragma once
#include "Export.h"
#include "Resource.h"
#include "RenderShared.h"
#include "ResourceManager.h"

namespace MMMEngine {
	class Material;
	class MMMENGINE_API SkeletalMesh : public Resource
	{
		RTTR_ENABLE(Resource);
		RTTR_REGISTRATION_FRIEND
			friend class ResourceManager;
			friend class SceneManager;
			friend class Scene;
	private:
		Microsoft::WRL::ComPtr<ID3D11Buffer> CreateVertexBuffer(const std::vector<Mesh_Vertex>& _vertices);
		Microsoft::WRL::ComPtr<ID3D11Buffer> CreateIndexBuffer(const std::vector<UINT>& _indices);
	public:
		// -- 직렬화되는 데이터
		// 메시 데이터
		MeshData meshData;
		// GPU 버퍼
		MeshGPU gpuBuffer;
		// 메테리얼
		std::vector<ResPtr<Material>> materials;
		// 메시 그룹 <MatIdx, MeshIdx>
		std::unordered_map<UINT, std::vector<UINT>> meshGroupData;
		// 인덱스 사이즈
		std::vector<UINT> indexSizes;

		// 본 버퍼
		Mesh_BoneBuffer offsetBuffer;
		// 본 그룹 <Name, BoneIdx>
		std::unordered_map<std::string, UINT> boneIdxData;
		// 노드 정보 <BoneIdx, NodeIdx>
		std::unordered_map<UINT, UINT> nodeIdxData;

		// 노드 트리
		NodeTreeAsset mNodeTree;

		bool LoadFromFilePath(const std::wstring& filePath) override;
	};
}


