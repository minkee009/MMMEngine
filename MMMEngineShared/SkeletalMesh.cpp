#include "SkeletalMesh.h"

#include "ResourceSerializer.h"
#include "RenderManager.h"
#include "RendererTools.h"
#include "Material.h"

#include <rttr/registration.h>
#pragma warning(disable: 4506)

RTTR_REGISTRATION
{
	using namespace rttr;
	using namespace MMMEngine;

	registration::class_<SkeletalMesh>("SkeletalMesh")
		.constructor<>()(rttr::policy::ctor::as_std_shared_ptr)
		.property("meshData", &SkeletalMesh::meshData)
		.property("meshGroupData", &SkeletalMesh::meshGroupData)
		.property("offsetBuffer", &SkeletalMesh::offsetBuffer);

	type::register_converter_func(
		[](std::shared_ptr<Resource> from, bool& ok) -> std::shared_ptr<SkeletalMesh>
		{
			if (!from) {  // nullptr 허용
				ok = true;
				return nullptr;
			}
			auto result = std::dynamic_pointer_cast<SkeletalMesh>(from);
			ok = (result != nullptr);
			return result;
		}
	);
}

Microsoft::WRL::ComPtr<ID3D11Buffer> MMMEngine::SkeletalMesh::CreateVertexBuffer(const std::vector<MMMEngine::Mesh_Vertex>& _vertices)
{
	// 예외 확인
	if (_vertices.empty())
		return nullptr;

	// 출력할 버퍼 생성
	Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;

	// 버텍스 버퍼 생성
	D3D11_BUFFER_DESC bd = {};
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.CPUAccessFlags = 0;
	bd.MiscFlags = 0;

	D3D11_SUBRESOURCE_DATA vbData = {};
	bd.ByteWidth = UINT(sizeof(MMMEngine::Mesh_Vertex) * _vertices.size());
	vbData.pSysMem = _vertices.data();

	MMMEngine::HR_T(MMMEngine::RenderManager::Get().GetDevice()->CreateBuffer(&bd, &vbData, buffer.GetAddressOf()));

	if (!buffer)
		throw std::runtime_error("SkeletalMesh::Creating VertexBuffer Failed !!");

	return buffer;
}

Microsoft::WRL::ComPtr<ID3D11Buffer> MMMEngine::SkeletalMesh::CreateIndexBuffer(const std::vector<UINT>& _indices)
{
	// 출력할 버퍼 생성
	Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;

	if (_indices.empty())
		return nullptr; // 안전 처리

	// 인덱스 버퍼 생성
	D3D11_BUFFER_DESC bd = {};
	bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.CPUAccessFlags = 0;
	bd.ByteWidth = UINT(sizeof(UINT) * _indices.size());

	D3D11_SUBRESOURCE_DATA ibData = {};
	ibData.pSysMem = _indices.data();

	MMMEngine::HR_T(
		MMMEngine::RenderManager::Get().GetDevice()->CreateBuffer(&bd, &ibData, buffer.GetAddressOf())
	);

	if (!buffer)
		throw std::runtime_error("SkeletalMesh::Creating IndexBuffer Failed !!");

	return buffer;

}

bool MMMEngine::SkeletalMesh::LoadFromFilePath(const std::wstring& filePath)
{
	if (!std::filesystem::exists(std::filesystem::path(filePath))) {
		std::cout << "SkeletalMesh::File does not exist!!" << std::endl;
		return false;
	}

	// 역직렬화
	ResourceSerializer::Get().DeSerialize_SkeletalMesh(this, filePath);

	// 버퍼 만들기
	for (auto& submesh : meshData.vertices) {
		Microsoft::WRL::ComPtr<ID3D11Buffer> subMeshBuffer = CreateVertexBuffer(submesh);
		gpuBuffer.vertexBuffers.push_back(subMeshBuffer);
	}
	for (auto& indices : meshData.indices) {
		Microsoft::WRL::ComPtr<ID3D11Buffer> subMeshBuffer = CreateIndexBuffer(indices);
		gpuBuffer.indexBuffers.push_back(subMeshBuffer);
		indexSizes.push_back(indices.size());
	}

	// VS 교체
	for (auto& material : materials) {
		std::filesystem::path vsPath{ material->GetVShader()->GetFilePath() };
		if (vsPath.empty()) {
			std::cout << "SkeletalMesh::VShader's FilesPath does not Exist !!" << std::endl;
			continue;
		}
		material->SetVShader(ShaderInfo::Get().GetSkeletalVShader());
	}

	// CPU 데이터 정리
	// WARNING::필요하면 지우거나 주석처리할것 (런타임 메모리 최적화용)
	//meshData.vertices.clear();
	//meshData.indices.clear();

	return true;
}
