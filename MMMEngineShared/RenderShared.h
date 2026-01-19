#pragma once
#include <SimpleMath.h>
#include <memory>
#include <Material.h>

#define BONE_MAXSIZE 256

enum RenderType {
	SHADOWMAP = 0,
	PREDEPTH = 1,
	SKYBOX = 2,
	TRANSCULANT = 3,
	GEOMETRYY = 4,
	ADDTIVE = 5,
	PARTICLE = 6,
	POSTPROCESS = 7,
	UI = 8,
};

struct Render_CamBuffer {
	DirectX::SimpleMath::Matrix mView = DirectX::SimpleMath::Matrix::Identity;			// 카메라좌표계 변환행렬
	DirectX::SimpleMath::Matrix mProjection = DirectX::SimpleMath::Matrix::Identity;	// ndc좌표계 변환행렬
	DirectX::SimpleMath::Vector4 camPos;
};

struct Render_TransformBuffer
{
	DirectX::SimpleMath::Matrix mWorld;
	DirectX::SimpleMath::Matrix mNormalMatrix;
};

struct Render_LightBuffer {
	DirectX::SimpleMath::Vector4 mLightDir;
	DirectX::SimpleMath::Vector4 mLightColor;
};

struct Render_ShadowBuffer
{
	DirectX::SimpleMath::Matrix ShadowView;
	DirectX::SimpleMath::Matrix ShadowProjection;
};

struct Mesh_Vertex
{
	DirectX::SimpleMath::Vector3 Pos;		// 정점 위치 정보
	DirectX::SimpleMath::Vector3 Normal;	// 노멀
	DirectX::SimpleMath::Vector3 Tangent;	// 탄젠트
	DirectX::SimpleMath::Vector2 UV;		// 텍스쳐 UV
	int boneIndices[4] = { -1, -1, -1, -1 };				// 버텍스와 연결된 본들의 인덱스
	float boneWeights[4] = { 0.0f, 0.0f, 0.0f, 0.0f };		// 각 본들의 가중치
};

struct Mesh_BoneBuffer
{
	DirectX::SimpleMath::Matrix boneMat[BONE_MAXSIZE] = { DirectX::SimpleMath::Matrix::Identity, };
};

struct MeshData {
	std::vector<std::vector<Mesh_Vertex>> vertices;
	std::vector<std::vector<UINT>> indices;
	std::vector<std::shared_ptr<MMMEngine::Material>> materials;
};

struct MeshGPU {
	std::vector<Microsoft::WRL::ComPtr<ID3D11Buffer>> vertexBuffers;	// 버텍스 버퍼 (idx 메시그룹
	std::vector<Microsoft::WRL::ComPtr<ID3D11Buffer>> indexBuffers;		// 인덱스 버퍼
};

struct MeshMatGroup {
	UINT materialIdx = 0;		// 메테리얼 인덱스
	std::vector<UINT> meshIdx;	// 메테리얼에 소속된 메시들 인덱스
};

struct MaterialInfo {
	std::vector<std::pair<std::wstring, std::wstring>> texProperties;	// propertyName, path
	std::
};