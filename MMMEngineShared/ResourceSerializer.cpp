#include "ResourceSerializer.h"
#include "json/json.hpp"
#include "rttr/type"

#include <string>
#include <fstream>

#include "StaticMesh.h"
#include "SkeletalMesh.h"
#include "AnimationClip.h"
#include "RenderShared.h"
#include "StringHelper.h"
#include "MaterialSerializer.h"

DEFINE_SINGLETON(MMMEngine::ResourceSerializer);

namespace fs = std::filesystem;
using json = nlohmann::json;
using namespace MMMEngine;
using namespace rttr;


json SerializeRttrVariant(const rttr::variant& var) {
	type t = var.get_type();
	

	if (t.is_wrapper())
	{
		auto wrapped = var.extract_wrapped_value();
		return SerializeRttrVariant(wrapped);
	}

	if (t.is_arithmetic()) {
		if (t == type::get<bool>()) return var.to_bool();
		if (t == type::get<int>()) return var.to_int();
		if (t == type::get<unsigned int>()) return var.to_uint32();
		if (t == type::get<long long>()) return var.to_int64();
		if (t == type::get<uint64_t>()) return var.to_uint64();
		if (t == type::get<float>()) return var.to_float();
		if (t == type::get<double>()) return var.to_double();
	}

	if (t == type::get<std::string>())
	{
		return var.to_string();
	}

	if (t.is_sequential_container())
	{
		json arr = json::array();
		auto view = var.create_sequential_view();
		for (const auto& item : view)
		{
			if (!item.is_valid())
				continue;

			json val = SerializeRttrVariant(item);

			// null 값은 추가하지 않음
			if (!val.is_null())
				arr.push_back(val);
		}
		return arr;
	}

	if (t.is_class()) {
		// 최적화용 한방배열
		// Vector2 → 배열 [x,y]
        if (t == type::get<DirectX::SimpleMath::Vector2>()) {
            auto vec = var.get_value<DirectX::SimpleMath::Vector2>();
            return json::array({ vec.x, vec.y });
        }
        // Vector3 → 배열 [x,y,z]
        if (t == type::get<DirectX::SimpleMath::Vector3>()) {
            auto vec = var.get_value<DirectX::SimpleMath::Vector3>();
            return json::array({ vec.x, vec.y, vec.z });
        }
		// Matrix → 배열 [x,y,z]
		if (t == type::get<DirectX::SimpleMath::Matrix>()) {
			auto mat = var.get_value<DirectX::SimpleMath::Matrix>();
			return json::array({
				mat._11, mat._12, mat._13, mat._14,
				mat._21, mat._22, mat._23, mat._24,
				mat._31, mat._32, mat._33, mat._34,
				mat._41, mat._42, mat._43, mat._44
				});
		}

		json obj;
		for (auto& prop : t.get_properties(
			rttr::filter_item::instance_item |
			rttr::filter_item::public_access |
			rttr::filter_item::non_public_access))
		{
			rttr::variant value = prop.get_value(var);
			if (value.is_valid()) {
				json val = SerializeRttrVariant(value);
				if (!val.is_null())
					obj[prop.get_name().to_string()] = val;
			}
		}
		return obj;
	}

	return {};
}

json SerializeVertex(const std::vector<Mesh_Vertex>& _vertices)
{
	json meshJson;
	type subt = type::get(_vertices);

	for (auto& vertex : _vertices)
	{
		type vert = type::get(vertex);
		json vertJson;

		for (auto& prop : vert.get_properties(
			rttr::filter_item::instance_item |
			rttr::filter_item::public_access |
			rttr::filter_item::non_public_access))
		{
			if (prop.is_readonly())
				continue;

			rttr::variant var = prop.get_value(vertex);
			vertJson[prop.get_name().to_string()] = SerializeRttrVariant(var);
		}
		meshJson.push_back(vertJson);
	}

	return meshJson;
}

json SerializeMesh(const MeshData& _meshData)
{
	json meshJson;

	json vertexJson = json::array();
	for (auto& vSubMesh : _meshData.vertices) {
		vertexJson.push_back(SerializeVertex(vSubMesh));
	}

	json indexJson = json::array();
	for (const auto& iSubMesh : _meshData.indices) {
		json arr = json::array();
		for (UINT val : iSubMesh)
			arr.push_back(val);
		indexJson.push_back(arr);
	}
	
	meshJson["Vertices"] = vertexJson;
	meshJson["Indices"] = indexJson;

	return meshJson;
}

json SerializeMeshGroup(const std::unordered_map<UINT, std::vector<UINT>>& meshGroupData)
{
	json out;
	for (const auto& [mat, meshArr] : meshGroupData) {
		out[std::to_string(mat)] = meshArr;
	}

	return out;
}

json SerializeBoneGroup(const std::unordered_map<std::string, UINT>& _boneGroupData)
{
	json boneGroup = json::array();
	for (const auto& [name, idx] : _boneGroupData) {
		json boneData = json::array();
		boneData.push_back(name);
		boneData.push_back(idx);

		boneGroup.push_back(boneData);
	}

	return boneGroup;
}

json SerializeNodeGroup(const std::unordered_map<UINT, UINT>& _NodeIdxData)
{
	json nodeGroup = json::array();
	for (const auto& [bIdx, nIdx] : _NodeIdxData) {
		json boneData = json::array();
		boneData.push_back(bIdx);
		boneData.push_back(nIdx);

		nodeGroup.push_back(boneData);
	}

	return nodeGroup;
}

json SerializeBoneOffset(const std::array<DirectX::SimpleMath::Matrix, BONE_MAXSIZE>& _offsets)
{
	json offsetGroup = json::array();
	DirectX::SimpleMath::Matrix lastMat = DirectX::SimpleMath::Matrix::Identity;
	for (const auto& mat : _offsets) {
		// 유효한 본이 끝난거같을때 중지
		if (lastMat == mat && mat == DirectX::SimpleMath::Matrix::Identity)
			break;
		lastMat = mat;

		rttr::variant var = mat;
		offsetGroup.push_back(SerializeRttrVariant(var));
	}

	return offsetGroup;
}

fs::path MMMEngine::ResourceSerializer::Serialize_StaticMesh(const StaticMesh* _in, std::wstring _path, std::wstring _name)
{
	json snapshot;

	auto meshMUID = _in->GetMUID().IsEmpty() ? Utility::MUID::NewMUID() : _in->GetMUID();

	snapshot["MUID"] = meshMUID.ToString();

	json matJson = json::array(); 
	int index = 0;
	for (auto& matPtr : _in->materials) {
		fs::path matPath = MaterialSerializer::Get().Serialize(matPtr.get(), _path, _name, index);
		++index;

		matJson.push_back(matPath.u8string());
	}
	snapshot["Materials"] = matJson;

	json meshJson = json::array();
	meshJson.push_back(SerializeMesh(_in->meshData));
	snapshot["Mesh"] = meshJson;

	json meshgroupJson;
	meshgroupJson.push_back(SerializeMeshGroup(_in->meshGroupData));
	snapshot["MeshGroup"] = meshgroupJson;

	// Json 출력
	std::vector<uint8_t> v = json::to_msgpack(snapshot);

	fs::path p(ResourceManager::Get().GetCurrentRootPath());
	p = p / _path;
	p = p / (_name.append(L"_StaticMesh.staticmesh"));

	if (p.has_parent_path() && !fs::exists(p.parent_path())) {
		fs::create_directories(p.parent_path());
	}

	std::ofstream file(p.string(), std::ios::binary);
	//std::ofstream file(p.string());
	if (!file.is_open()) {
		throw std::runtime_error("파일을 열 수 없습니다: " + Utility::StringHelper::WStringToString(_path));
	}

	/*file << snapshot.dump(4);
	file.close();*/

	file.write(reinterpret_cast<const char*>(v.data()), v.size());
	file.close();

	return p;
}

void MMMEngine::ResourceSerializer::DeSerialize_StaticMesh(StaticMesh* _out, std::wstring _path)
{
	//// 파일 열기
	//std::ifstream file(_path, std::ios::binary);
	//if (!file.is_open())
	//{
	//	throw std::runtime_error("파일을 열 수 없습니다: " + Utility::StringHelper::WStringToString(_path));
	//}

	//// 파일 전체 읽기
	//std::vector<uint8_t> buffer((std::istreambuf_iterator<char>(file)),
	//	std::istreambuf_iterator<char>());
	std::ifstream file(_path, std::ios::binary | std::ios::ate);
	auto size = file.tellg();
	file.seekg(0, std::ios::beg);

	std::vector<char> buffer(size);
	file.read(buffer.data(), size);
	file.close();

	// msgpack → json 변환
	json snapshot = json::from_msgpack(buffer);

	// MUID 복원 ( 아직 안씀)
	/*if (snapshot.contains("MUID"))
	{
		const_cast<StaticMesh*>(_out)->SetMUID(muid);ss
	}*/

	// Materials 복원
	if (snapshot.contains("Materials"))
	{
		auto& matJson = snapshot["Materials"];
		std::vector<ResPtr<Material>> mats;
		for (auto& m : matJson)
		{
			fs::path basePath(ResourceManager::Get().GetCurrentRootPath());
			fs::path matPath(_path);
			matPath = matPath.parent_path();
			matPath = matPath / m.get<std::string>();

			matPath = matPath.lexically_relative(basePath);

			auto mat = ResourceManager::Get().Load<Material>(matPath.wstring());
			mats.push_back(mat);
		}
		_out->materials.swap(mats);
	}

	// Mesh 복원
	if (snapshot.contains("Mesh")) {
		auto& meshJsonArr = snapshot["Mesh"];
		if (!meshJsonArr.empty()) {
			auto& meshJson = meshJsonArr[0];
			MeshData meshData;

			// Vertices 복원
			if (meshJson.contains("Vertices")) {
				for (auto& subMeshJson : meshJson["Vertices"]) {
					std::vector<Mesh_Vertex> subMesh;
					for (auto& vertJson : subMeshJson) {
						Mesh_Vertex vertex;

						// RTTR 파싱
						//type vertType = type::get<Mesh_Vertex>();
						//for (auto& prop : vertType.get_properties()) {
						//	if (prop.is_readonly())
						//		continue;

						//	std::string name = prop.get_name().to_string();
						//	if (!vertJson.contains(name))
						//		continue;

						//	auto& jval = vertJson[name];
						//	rttr::variant newVal;

						//	// 숫자/문자/배열 처리
						//	if (jval.is_number_integer())
						//		newVal = jval.get<int>();
						//	else if (jval.is_number_float())
						//		newVal = jval.get<float>();
						//	else if (jval.is_string())
						//		newVal = jval.get<std::string>();
						//	else if (jval.is_array() && jval.size() == 3) {
						//		DirectX::SimpleMath::Vector3 vec;
						//		vec.x = jval[0].get<float>();
						//		vec.y = jval[1].get<float>();
						//		vec.z = jval[2].get<float>();
						//		newVal = vec;
						//	}
						//	else if (jval.is_array() && jval.size() == 2) {
						//		DirectX::SimpleMath::Vector2 vec;
						//		vec.x = jval[0].get<float>();
						//		vec.y = jval[1].get<float>();
						//		newVal = vec;
						//	}
						//	else if (jval.is_array()) {
						//		std::vector<int> arr;
						//		for (auto& elem : jval)
						//			arr.push_back(elem.get<int>());
						//		newVal = arr;
						//	}

						//	if (newVal.is_valid())
						//		prop.set_value(vertex, newVal);
						//}

						// 직접 파싱
						vertex.Pos = { vertJson["Pos"][0], vertJson["Pos"][1], vertJson["Pos"][2] };
						vertex.Normal = { vertJson["Normal"][0], vertJson["Normal"][1], vertJson["Normal"][2] };
						vertex.Tangent = { vertJson["Tangent"][0], vertJson["Tangent"][1], vertJson["Tangent"][2] };
						vertex.UV = { vertJson["UV"][0], vertJson["UV"][1] };
						subMesh.push_back(vertex);
					}
					meshData.vertices.push_back(subMesh);
				}
			}

			// Indices 복원
			if (meshJson.contains("Indices")) {
				for (auto& iSubMeshJson : meshJson["Indices"]) {
					std::vector<UINT> indices;
					for (auto& elem : iSubMeshJson) {
						indices.push_back(elem.get<UINT>());
					}
					meshData.indices.push_back(indices);
				}
			}

			_out->meshData = std::move(meshData);
		}
	}


	// MeshGroup 복원
	if (snapshot.contains("MeshGroup"))
	{
		auto& meshGroupJsonArr = snapshot["MeshGroup"];
		if (!meshGroupJsonArr.empty()) {
			auto& meshGroupJson = meshGroupJsonArr[0];
			std::unordered_map<UINT, std::vector<UINT>> data;
			for (auto& [key, value] : meshGroupJson.items()) {
				if (key == "Type") continue;
				UINT matId = static_cast<UINT>(std::stoul(key));
				data[matId] = value.get<std::vector<UINT>>();
			}
			_out->meshGroupData = std::move(data);
		}
	}
}

std::filesystem::path MMMEngine::ResourceSerializer::Serialize_SkeletalMesh(const SkeletalMesh* _in, std::wstring _path, std::wstring _name)
{
	json snapshot;

	auto meshMUID = _in->GetMUID().IsEmpty() ? Utility::MUID::NewMUID() : _in->GetMUID();

	snapshot["MUID"] = meshMUID.ToString();

	json matJson = json::array();
	int index = 0;
	for (auto& matPtr : _in->materials) {
		fs::path matPath = MaterialSerializer::Get().Serialize(matPtr.get(), _path, _name, index);
		++index;

		matJson.push_back(matPath.u8string());
	}
	snapshot["Materials"] = matJson;

	json meshJson = json::array();
	meshJson.push_back(SerializeMesh(_in->meshData));
	snapshot["Mesh"] = meshJson;

	json meshgroupJson;
	meshgroupJson.push_back(SerializeMeshGroup(_in->meshGroupData));
	snapshot["MeshGroup"] = meshgroupJson;

	// 스킨드메시용
	json BoneIdxDataJson;
	BoneIdxDataJson.push_back(SerializeBoneGroup(_in->boneIdxData));
	snapshot["BoneIdxData"] = BoneIdxDataJson;

	json nodeIdxDataJson;
	nodeIdxDataJson.push_back(SerializeNodeGroup(_in->nodeIdxData));
	snapshot["NodeIdxData"] = nodeIdxDataJson;

	json boneOffsetJson;
	boneOffsetJson.push_back(SerializeBoneOffset(_in->offsetBuffer.BoneMat));
	snapshot["BoneOffset"] = boneOffsetJson;

	// NodeTree 직렬화
	snapshot["NodeTree"]["RootIndex"] = std::to_string(_in->mNodeTree.rootIndex);
	snapshot["NodeTree"]["Nodes"] = json::array();
	for (const auto& node : _in->mNodeTree.nodes)
	{
		json n;
		n["Name"] = node.name;
		n["ParentIndex"] = node.parentIndex;
		n["Children"] = node.children;

		// bindLocal 행렬을 배열로 저장
		n["BindLocal"] = {
			node.bindLocal._11, node.bindLocal._12, node.bindLocal._13, node.bindLocal._14,
			node.bindLocal._21, node.bindLocal._22, node.bindLocal._23, node.bindLocal._24,
			node.bindLocal._31, node.bindLocal._32, node.bindLocal._33, node.bindLocal._34,
			node.bindLocal._41, node.bindLocal._42, node.bindLocal._43, node.bindLocal._44
		};

		snapshot["NodeTree"]["Nodes"].push_back(n);
	}

	// Json 출력
	std::vector<uint8_t> v = json::to_msgpack(snapshot);

	fs::path p(ResourceManager::Get().GetCurrentRootPath());
	p = p / _path;
	p = p / (_name.append(L"_SkinMesh.skinmesh"));

	if (p.has_parent_path() && !fs::exists(p.parent_path())) {
		fs::create_directories(p.parent_path());
	}

	std::ofstream file(p.string(), std::ios::binary);
	//std::ofstream file(p.string());
	if (!file.is_open()) {
		throw std::runtime_error("파일을 열 수 없습니다: " + Utility::StringHelper::WStringToString(_path));
	}

	/*file << snapshot.dump(4);
	file.close();*/

	file.write(reinterpret_cast<const char*>(v.data()), v.size());
	file.close();

	return p;
}

void MMMEngine::ResourceSerializer::DeSerialize_SkeletalMesh(SkeletalMesh* _out, std::wstring _path)
{
	// 파일 읽기 (StaticMesh와 동일 패턴)
	std::ifstream file(_path, std::ios::binary | std::ios::ate);
	if (!file.is_open())
	{
		throw std::runtime_error("파일을 열 수 없습니다: " + Utility::StringHelper::WStringToString(_path));
	}

	auto size = file.tellg();
	file.seekg(0, std::ios::beg);

	std::vector<char> buffer(static_cast<size_t>(size));
	file.read(buffer.data(), size);
	file.close();

	// msgpack -> json
	json snapshot = json::from_msgpack(buffer);

	// MUID 복원 (필요시)
	// if (snapshot.contains("MUID")) { ... }

	// Materials 복원
	if (snapshot.contains("Materials"))
	{
		auto& matJson = snapshot["Materials"];
		std::vector<ResPtr<Material>> mats;

		for (auto& m : matJson)
		{
			fs::path basePath(ResourceManager::Get().GetCurrentRootPath());
			fs::path meshPath(_path);
			meshPath = meshPath.parent_path();

			fs::path matPath = meshPath / m.get<std::string>();
			matPath = matPath.lexically_relative(basePath);

			auto mat = ResourceManager::Get().Load<Material>(matPath.wstring());
			mats.push_back(mat);
		}

		_out->materials = std::move(mats);
	}

	// Mesh 복원 (StaticMesh와 동일)
	if (snapshot.contains("Mesh"))
	{
		auto& meshJsonArr = snapshot["Mesh"];
		if (!meshJsonArr.empty())
		{
			auto& meshJson = meshJsonArr[0];
			MeshData meshData;

			// Vertices
			if (meshJson.contains("Vertices"))
			{
				for (auto& subMeshJson : meshJson["Vertices"])
				{
					std::vector<Mesh_Vertex> subMesh;
					for (auto& vertJson : subMeshJson)
					{
						Mesh_Vertex vertex;
						vertex.Pos = { vertJson["Pos"][0],    vertJson["Pos"][1],    vertJson["Pos"][2] };
						vertex.Normal = { vertJson["Normal"][0], vertJson["Normal"][1], vertJson["Normal"][2] };
						vertex.Tangent = { vertJson["Tangent"][0],vertJson["Tangent"][1],vertJson["Tangent"][2] };
						vertex.UV = { vertJson["UV"][0],     vertJson["UV"][1] };
						vertex.BoneIndices = { vertJson["BoneIndices"][0], vertJson["BoneIndices"][1], vertJson["BoneIndices"][2], vertJson["BoneIndices"][3] };
						vertex.BoneWeights = { vertJson["BoneWeights"][0], vertJson["BoneWeights"][1], vertJson["BoneWeights"][2], vertJson["BoneWeights"][3] };

						subMesh.push_back(vertex);
					}
					meshData.vertices.push_back(subMesh);
				}
			}

			// Indices
			if (meshJson.contains("Indices"))
			{
				for (auto& iSubMeshJson : meshJson["Indices"])
				{
					std::vector<UINT> indices;
					for (auto& elem : iSubMeshJson)
						indices.push_back(elem.get<UINT>());
					meshData.indices.push_back(indices);
				}
			}

			_out->meshData = std::move(meshData);
		}
	}

	// MeshGroup 복원 (StaticMesh와 동일)
	if (snapshot.contains("MeshGroup"))
	{
		auto& meshGroupJsonArr = snapshot["MeshGroup"];
		if (!meshGroupJsonArr.empty())
		{
			auto& meshGroupJson = meshGroupJsonArr[0];

			std::unordered_map<UINT, std::vector<UINT>> data;
			for (auto& [key, value] : meshGroupJson.items())
			{
				if (key == "Type") continue;
				UINT matId = static_cast<UINT>(std::stoul(key));
				data[matId] = value.get<std::vector<UINT>>();
			}
			_out->meshGroupData = std::move(data);
		}
	}

	// BoneIdxData 복원
	if (snapshot.contains("BoneIdxData"))
	{
		auto& boneGroupArr = snapshot["BoneIdxData"];
		if (!boneGroupArr.empty())
		{
			auto& boneGroup = boneGroupArr[0]; // SerializeBoneGroup에서 array로 감쌌으니 [0]

			std::unordered_map<std::string, UINT> bg;
			// boneGroup: [ [name, idx], [name, idx], ... ]
			for (auto& pair : boneGroup)
			{
				if (!pair.is_array() || pair.size() < 2) continue;
				std::string name = pair[0].get<std::string>();
				UINT idx = pair[1].get<UINT>();
				bg[name] = idx;
			}

			_out->boneIdxData = std::move(bg);
		}
	}

	// NodeIdxData 복원
	if (snapshot.contains("NodeIdxData"))
	{
		auto& nodeGroupArr = snapshot["NodeIdxData"];
		if (!nodeGroupArr.empty())
		{
			auto& boneGroup = nodeGroupArr[0]; // SerializeBoneGroup에서 array로 감쌌으니 [0]

			std::unordered_map<UINT, UINT> bg;
			// boneGroup: [ [name, idx], [name, idx], ... ]
			for (auto& pair : boneGroup)
			{
				if (!pair.is_array() || pair.size() < 2) continue;
				UINT bIdx = pair[0].get<UINT>();
				UINT nIdx = pair[1].get<UINT>();
				bg[bIdx] = nIdx;
			}

			_out->nodeIdxData = std::move(bg);
		}
	}

	// BoneOffset 복원
	if (snapshot.contains("BoneOffset"))
	{
		auto& boneOffsetArr = snapshot["BoneOffset"];
		if (!boneOffsetArr.empty())
		{
			auto& offsetGroup = boneOffsetArr[0]; // SerializeBoneOffset에서 array로 감쌌으니 [0]

			// 일단 Identity로 초기화
			for (auto& m : _out->offsetBuffer.BoneMat)
				m = DirectX::SimpleMath::Matrix::Identity;

			size_t i = 0;
			for (auto& elem : offsetGroup)
			{
				if (i >= BONE_MAXSIZE) break;

				if (elem.is_array() && elem.size() == 16)
				{
					DirectX::SimpleMath::Matrix m;
					m._11 = elem[0].get<float>();  m._12 = elem[1].get<float>();  m._13 = elem[2].get<float>();  m._14 = elem[3].get<float>();
					m._21 = elem[4].get<float>();  m._22 = elem[5].get<float>();  m._23 = elem[6].get<float>();  m._24 = elem[7].get<float>();
					m._31 = elem[8].get<float>();  m._32 = elem[9].get<float>();  m._33 = elem[10].get<float>(); m._34 = elem[11].get<float>();
					m._41 = elem[12].get<float>(); m._42 = elem[13].get<float>(); m._43 = elem[14].get<float>(); m._44 = elem[15].get<float>();
					_out->offsetBuffer.BoneMat[i] = m;
				}
				++i;
			}
		}
	}

	// NodeTree 역직렬화
	_out->mNodeTree.rootIndex = std::stoi(snapshot["NodeTree"]["RootIndex"].get<std::string>());
	_out->mNodeTree.nodes.clear();
	_out->mNodeTree.nodeIndexByName.clear();

	for (auto& n : snapshot["NodeTree"]["Nodes"])
	{
		NodeData node;
		node.name = n["Name"].get<std::string>();
		node.parentIndex = n["ParentIndex"].get<int>();
		node.children = n["Children"].get<std::vector<int>>();

		auto matArr = n["BindLocal"];
		node.bindLocal = DirectX::SimpleMath::Matrix(
			matArr[0], matArr[1], matArr[2], matArr[3],
			matArr[4], matArr[5], matArr[6], matArr[7],
			matArr[8], matArr[9], matArr[10], matArr[11],
			matArr[12], matArr[13], matArr[14], matArr[15]
		);

		_out->mNodeTree.nodes.push_back(node);
		_out->mNodeTree.nodeIndexByName[node.name] = static_cast<int>(_out->mNodeTree.nodes.size() - 1);
	}
}

std::filesystem::path MMMEngine::ResourceSerializer::Serialize_Animation(const AnimationClip* _in, std::wstring _path, std::wstring _name, int _idx)
{
	json j;
	j["name"] = _in->mName;
	j["durationSec"] = _in->durationSec;
	j["ticksPerSecond"] = _in->ticksPerSecond;

	// AnimTrack 직렬화
	j["tracks"] = json::array();
	for (const auto& track : _in->mTracks)
	{
		json t;
		t["nodeIndex"] = track.nodeIndex;

		t["posKeys"] = json::array();
		for (const auto& k : track.posKeys)
			t["posKeys"].push_back({ {"timeSec", k.timeSec}, {"value", {k.value.x, k.value.y, k.value.z}} });

		t["rotKeys"] = json::array();
		for (const auto& k : track.rotKeys)
			t["rotKeys"].push_back({ {"timeSec", k.timeSec}, {"value", {k.value.x, k.value.y, k.value.z, k.value.w}} });

		t["scaleKeys"] = json::array();
		for (const auto& k : track.scaleKeys)
			t["scaleKeys"].push_back({ {"timeSec", k.timeSec}, {"value", {k.value.x, k.value.y, k.value.z}} });

		j["tracks"].push_back(t);
	}

	// 파일 경로 생성
	fs::path rootPath(ResourceManager::Get().GetCurrentRootPath());
	fs::path outPath(_path);
	outPath = outPath / std::wstring(_name + L"_" + std::to_wstring(_idx) + L".animclip");

	fs::path finalPath = rootPath / outPath;

	// JSON 파일 저장
	std::ofstream ofs(finalPath);
	ofs << j.dump(4);
	ofs.close();

	return outPath;
}

void MMMEngine::ResourceSerializer::DeSerialize_Animation(AnimationClip* _out, std::wstring _path)
{
	// wide string → narrow string 변환
	std::string path(Utility::StringHelper::WStringToString(_path));

	std::ifstream ifs(path);
	if (!ifs.is_open())
		return;

	json j;
	ifs >> j;

	// 기본 정보
	_out->mName = j["name"].get<std::string>();
	_out->durationSec = j["durationSec"].get<float>();
	_out->ticksPerSecond = j["ticksPerSecond"].get<float>();

	// AnimTrack 역직렬화
	_out->mTracks.clear();
	for (auto& t : j["tracks"])
	{
		Mesh_AnimTrack track;
		track.nodeIndex = t["nodeIndex"].get<int>();

		for (auto& pk : t["posKeys"])
		{
			Mesh_VecKey key;
			key.timeSec = pk["timeSec"].get<float>();
			key.value = DirectX::SimpleMath::Vector3(
				pk["value"][0], pk["value"][1], pk["value"][2]);
			track.posKeys.push_back(key);
		}

		for (auto& rk : t["rotKeys"])
		{
			Mesh_QuatKey key;
			key.timeSec = rk["timeSec"].get<float>();
			key.value = DirectX::SimpleMath::Vector4(
				rk["value"][0], rk["value"][1], rk["value"][2], rk["value"][3]);
			track.rotKeys.push_back(key);
		}

		for (auto& sk : t["scaleKeys"])
		{
			Mesh_VecKey key;
			key.timeSec = sk["timeSec"].get<float>();
			key.value = DirectX::SimpleMath::Vector3(
				sk["value"][0], sk["value"][1], sk["value"][2]);
			track.scaleKeys.push_back(key);
		}

		_out->mTracks.push_back(track);
	}
}
