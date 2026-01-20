#include "MaterialSerealizer.h"
#include <rttr/type>
#include <fstream>
#include <filesystem>

#include "VShader.h"
#include "PShader.h"
#include "StringHelper.h"

using json = nlohmann::json;
namespace fs = std::filesystem;

DEFINE_SINGLETON(MMMEngine::MaterialSerealizer);

MMMEngine::PropertyValue MMMEngine::MaterialSerealizer::property_from_json(const nlohmann::json& j)
{
	std::string type = j.at("type").get<std::string>();

	if (type == "int")
		return j.at("value").get<int>();
	else if (type == "float")
		return j.at("value").get<float>();
	else if (type == "Vector3")
	{
		auto arr = j.at("value");
		return DirectX::SimpleMath::Vector3(arr[0], arr[1], arr[2]);
	}
	else if (type == "Matrix")
	{
		auto arr = j.at("value");
		DirectX::SimpleMath::Matrix m;
		m._11 = arr[0]; m._12 = arr[1]; m._13 = arr[2]; m._14 = arr[3];
		m._21 = arr[4]; m._22 = arr[5]; m._23 = arr[6]; m._24 = arr[7];
		m._31 = arr[8]; m._32 = arr[9]; m._33 = arr[10]; m._34 = arr[11];
		m._41 = arr[12]; m._42 = arr[13]; m._43 = arr[14]; m._44 = arr[15];
		return m;
	}
	else if (type == "Texture2D")
	{
		std::wstring filePath = j.at("file").get<std::wstring>();
		auto tex = ResourceManager::Get().Load<Texture2D>(filePath);
		return tex;
	}

	throw std::runtime_error("Unknown PropertyValue type: " + type);
}

void MMMEngine::MaterialSerealizer::to_json(json& j, const MMMEngine::PropertyValue& value)
{
	std::visit([&](auto&& arg) {
		using T = std::decay_t<decltype(arg)>;

		if constexpr (std::is_same_v<T, int>)
			j = { {"type", "int"}, {"value", arg} };
		else if constexpr (std::is_same_v<T, float>)
			j = { {"type", "float"}, {"value", arg} };
		else if constexpr (std::is_same_v<T, DirectX::SimpleMath::Vector3>)
			j = { {"type", "Vector3"}, {"value", {arg.x, arg.y, arg.z}} };
		else if constexpr (std::is_same_v<T, DirectX::SimpleMath::Matrix>)
			j = { {"type", "Matrix"}, {"value", {
				arg._11,arg._12,arg._13,arg._14,
				arg._21,arg._22,arg._23,arg._24,
				arg._31,arg._32,arg._33,arg._34,
				arg._41,arg._42,arg._43,arg._44
			}} };
		else if constexpr (std::is_same_v<T, ResPtr<MMMEngine::Texture2D>>)
			j = { {"type", "Texture2D"}, {"file", arg ? arg->GetFilePath() : L""} };
		}, value);
}


void MMMEngine::MaterialSerealizer::Serealize(Material* _material, std::wstring _path)
{
	json snapshot;
	auto matMUID = _material->GetMUID().IsEmpty() ? Utility::MUID::NewMUID() : _material->GetMUID();

	snapshot["MUID"] = matMUID.ToString();
	snapshot["name"] = _material->GetName();

	json props = json::object();
	for (auto& [key, val] : _material->GetProperties())
	{
		std::string skey(Utility::StringHelper::WStringToString(key));
		to_json(props[skey], val);
	}
	snapshot["properties"] = props;
	snapshot["vshader"] = { "file", _material->GetVShader()->GetFilePath() };
	snapshot["pshader"] = { "file", _material->GetPShader()->GetFilePath() };
	
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

void MMMEngine::MaterialSerealizer::UnSerealize(Material* _material, std::wstring _path)
{
	// 파일 읽기
	std::ifstream inFile("data.json", std::ios::binary);
	if (!inFile.is_open()) {
		throw std::runtime_error("파일을 열수 없습니다.");
	}

	nlohmann::json snapshot;
	inFile >> snapshot;

	// Name
	if (snapshot.contains("name")) {
		std::wstring ws(snapshot["name"].get<std::string>().begin(), snapshot["name"].get<std::string>().end());
		_material->SetName(ws);
	}

	// Properties
	if (snapshot.contains("properties")) {
		for (auto& [key, val] : snapshot["properties"].items()) {
			std::wstring wkey(key.begin(), key.end());
			PropertyValue pv = property_from_json(val);
			_material->SetProperty(wkey, pv);
		}
	}

	// VShader
	if (snapshot.contains("vshader")) {
		std::wstring ws(snapshot["vshader"].get<std::string>().begin(), snapshot["vshader"].get<std::string>().end());
		_material->SetVShader(ws);
	}

	// PShader
	if (snapshot.contains("pshader")) {
		std::wstring ws(snapshot["pshader"].get<std::string>().begin(), snapshot["pshader"].get<std::string>().end());
		_material->SetPShader(ws);
	}
}
