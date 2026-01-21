#include "ResourceSerializer.h"
#include "json/json.hpp"
#include "StaticMesh.h"
#include "RenderShared.h"
#include "rttr/type"

#include <fstream>
#include <filesystem>
#include "StringHelper.h"

DEFINE_SINGLETON(MMMEngine::ResourceSerializer);

namespace fs = std::filesystem;
using json = nlohmann::json;
using namespace MMMEngine;
using namespace rttr;


json SerializeVariant(const rttr::variant& var) {
	type t = var.get_type();

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
			arr.push_back(SerializeVariant(item));
		}
		return arr;
	}

	if (t.is_class()) {
		json obj;
		for (auto& prop : t.get_properties(
			rttr::filter_item::instance_item |
			rttr::filter_item::public_access |
			rttr::filter_item::non_public_access))
		{
			rttr::variant value = prop.get_value(var);
			if (value.is_valid())
				obj[prop.get_name().to_string()] = SerializeVariant(value);
		}
		return obj;
	}

	return {};
}

json SerializeVertex(const std::vector<Mesh_Vertex>& _vertices)
{
	json meshJson;
	type subt = type::get(_vertices);
	meshJson["Type"] = subt.get_name().to_string();

	for (auto& vertex : _vertices)
	{
		type vert = type::get(vertex);
		json vertJson;
		vertJson["Type"] = vert.get_name().to_string();

		for (auto& prop : vert.get_properties(
			rttr::filter_item::instance_item |
			rttr::filter_item::public_access |
			rttr::filter_item::non_public_access))
		{
			auto string = vert.get_name().to_string();
			if (prop.is_readonly())
				continue;

			rttr::variant var = prop.get_value(vertex);
			vertJson[prop.get_name().to_string()] = SerializeVariant(var);
		}
		meshJson["SubMesh"].push_back(vertJson);
	}

	return meshJson;
}

json SerializeMesh(const MeshData& _meshData)
{
	json submeshJson = json::array();
	for (auto& subMesh : _meshData.vertices) {
		submeshJson.push_back(SerializeVertex(subMesh));
	}

	return submeshJson;
}

void MMMEngine::ResourceSerializer::Serialize_StaticMesh(const StaticMesh* _in, std::wstring _path)
{
	json snapshot;

	auto meshMUID = _in->GetMUID().IsEmpty() ? Utility::MUID::NewMUID() : _in->GetMUID();

	snapshot["MUID"] = meshMUID.ToString();

	json meshJson = json::array();
	meshJson.push_back(SerializeMesh(_in->meshData));
	snapshot["Mesh"] = meshJson;

	json matJson = json::array();
	for (auto& matPtr : _in->materials) {
		matJson.push_back(matPtr->GetFilePath());
	}
	snapshot["Materials"] = matJson;

	// Json 출력
	std::vector<uint8_t> v = json::to_msgpack(snapshot);

	fs::path p(_path);
	if (p.has_parent_path() && !fs::exists(p.parent_path())) {
		fs::create_directories(p.parent_path());
	}

	std::ofstream file(_path, std::ios::binary);
	if (!file.is_open()) {
		throw std::runtime_error("파일을 열 수 없습니다: " + Utility::StringHelper::WStringToString(_path));
	}

	file.write(reinterpret_cast<const char*>(v.data()), v.size());
	file.close();
}

void MMMEngine::ResourceSerializer::DeSerialize_StaticMesh(StaticMesh* _out, std::wstring _path)
{
	// 파일 열기
	std::ifstream file(_path, std::ios::binary);
	if (!file.is_open())
	{
		throw std::runtime_error("파일을 열 수 없습니다: " + Utility::StringHelper::WStringToString(_path));
	}

	// 파일 전체 읽기
	std::vector<uint8_t> buffer((std::istreambuf_iterator<char>(file)),
		std::istreambuf_iterator<char>());
	file.close();

	// msgpack → json 변환
	json snapshot = json::from_msgpack(buffer);

	// MUID 복원 ( 아직 안씀)
	/*if (snapshot.contains("MUID"))
	{
		const_cast<StaticMesh*>(_out)->SetMUID(muid);
	}*/

	// Mesh 복원
	if (snapshot.contains("Mesh"))
	{
		auto& meshJson = snapshot["Mesh"];
		MeshData meshData;

		for (auto& submeshJson : meshJson)
		{
			std::vector<Mesh_Vertex> subMesh;

			for (auto& vertJson : submeshJson["SubMesh"])
			{
				Mesh_Vertex v;
				rttr::type t = rttr::type::get<Mesh_Vertex>();

				for (auto& prop : t.get_properties())
				{
					std::string name = prop.get_name().to_string();
					if (!vertJson.contains(name))
						continue;

					// JSON → variant 변환
					rttr::variant newVal;
					auto& jval = vertJson[name];

					if (jval.is_number_integer())
						newVal = jval.get<int>();
					else if (jval.is_number_float())
						newVal = jval.get<float>();
					else if (jval.is_string())
						newVal = jval.get<std::string>();
					else if (jval.is_array())
					{
						// 배열 처리 (BoneIndices, BoneWeights)
						std::vector<int> arr;
						for (auto& elem : jval)
							arr.push_back(elem.get<int>());
						newVal = arr;
					}
					else if (jval.is_object())
					{
						// Vector2/Vector3 처리
						if (jval.contains("x") && jval.contains("y"))
						{
							if (jval.contains("z"))
							{
								DirectX::SimpleMath::Vector3 vec;
								vec.x = jval["x"].get<float>();
								vec.y = jval["y"].get<float>();
								vec.z = jval["z"].get<float>();
								newVal = vec;
							}
							else
							{
								DirectX::SimpleMath::Vector2 vec;
								vec.x = jval["x"].get<float>();
								vec.y = jval["y"].get<float>();
								newVal = vec;
							}
						}
					}

					if (newVal.is_valid())
						prop.set_value(v, newVal);
				}

				subMesh.push_back(v);
			}

			meshData.vertices.push_back(subMesh);
		}

		_out->meshData = std::move(meshData);
	}

	// Materials 복원
	if (snapshot.contains("Materials"))
	{
		auto& matJson = snapshot["Materials"];
		std::vector<ResPtr<Material>> mats;
		for (auto& m : matJson)
		{
			auto mat = std::make_shared<Material>();
			mat->LoadFromFilePath(Utility::StringHelper::StringToWString(m.get<std::string>()));
			mats.push_back(mat);
		}
		_out->materials = std::move(mats);
	}

}
