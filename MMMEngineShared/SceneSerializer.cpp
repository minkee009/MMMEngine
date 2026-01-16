#include "SceneSerializer.h"
#include "GameObject.h"
#include "Component.h"
#include "StringHelper.h"
#include "rttr/type"
#include "Transform.h"

#include <fstream>
#include <filesystem>

DEFINE_SINGLETON(MMMEngine::SceneSerializer)

namespace fs = std::filesystem;
using json = nlohmann::json;
using namespace MMMEngine;
using namespace rttr;

json SerializeVariant(const rttr::variant& var);
json SerializeObject(const rttr::instance& obj)
{
    json j;
    type t = obj.get_type();

    for (auto& prop : t.get_properties())
    {
        if (prop.is_readonly())
            continue;
        rttr::variant value = prop.get_value(obj);
        j[prop.get_name().to_string()] = SerializeVariant(value);
    }

    return j;
}

json SerializeVariant(const rttr::variant& var)
{
    rttr::type t = var.get_type();

    if (t.is_arithmetic())
    {
        if (t == type::get<bool>()) return var.to_bool();
        if (t == type::get<int>()) return var.to_int();
        if (t == type::get<unsigned int>()) return var.to_uint32();
        if (t == type::get<long long>()) return var.to_int64();
        if (t == type::get<uint64_t>()) return var.to_uint64(); // 핵심: uint64_t 추가
        if (t == type::get<float>()) return var.to_float();
        if (t == type::get<double>()) return var.to_double();
    }

    if (t == type::get<MMMEngine::Utility::MUID>()) {
        return var.get_value<MMMEngine::Utility::MUID>().ToString();
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

    if (var.get_type().get_name().to_string().find("ObjPtr") != std::string::npos)
    {
        MMMEngine::Object* obj = nullptr;
        if (var.convert(obj) && obj != nullptr)
        {
            return obj->GetMUID().ToString();
        }
        return nullptr;
    }

    if (t.is_associative_container())
    {
        json obj;
        auto view = var.create_associative_view();
        for (auto& item : view)
        {
            obj[SerializeObject(item.first).dump()] = SerializeObject(item.second);
        }
        return obj;
    }

    // 사용자 정의 타입 -> 재귀
    return SerializeObject(var);
}

json SerializeComponent(const ObjPtr<Component>& comp)
{
	json compJson;
	type type = type::get(*comp);
	compJson["Type"] = type.get_name().to_string();

	for (auto& prop : type.get_properties())
	{
        if (prop.is_readonly())
            continue;

		rttr::variant value = prop.get_value(*comp);
		compJson["Props"][prop.get_name().to_string()] = SerializeVariant(value);
	}

	return compJson;
}

void MMMEngine::SceneSerializer::Serialize(const Scene& scene, std::wstring path)
{
	json snapshot;

    auto sceneMUID = scene.GetMUID().IsEmpty() ? Utility::MUID::NewMUID() : scene.GetMUID();

	snapshot["MUID"] = sceneMUID.ToString();
	snapshot["Name"] = Utility::StringHelper::WStringToString(Utility::StringHelper::ExtractFileName(path));

	json goArray = json::array();

	for (auto& goPtr : scene.m_gameObjects)
	{
		if (!goPtr.IsValid())
			continue;

		json goJson;
		goJson["Name"] = goPtr->GetName();
		goJson["MUID"] = goPtr->GetMUID().ToString();

		json compArray = json::array();
		for (auto& comp : goPtr->GetAllComponents()) // 컴포넌트 리스트 가정
		{
			compArray.push_back(SerializeComponent(comp));
		}
		goJson["Components"] = compArray;

		goArray.push_back(goJson);
	}

	snapshot["GameObjects"] = goArray;
	std::vector<uint8_t> v = json::to_msgpack(snapshot);

    // 1. 경로 준비 (기존 코드 유지)
    fs::path p(path);
    if (p.has_parent_path() && !fs::exists(p.parent_path())) {
        fs::create_directories(p.parent_path());
    }

    // 2. 파일 쓰기 (텍스트 모드)
    std::ofstream file(path); // 바이너리 플래그 제거

    if (!file.is_open()) {
        throw std::runtime_error("파일을 열 수 없습니다: " + Utility::StringHelper::WStringToString(path));
    }

    // 3. snapshot.dump(들여쓰기_공간_수)를 사용하여 파일에 기록
    // dump(4)를 사용하면 4칸 들여쓰기가 적용되어 사람이 읽기 편해집니다.
    file << snapshot.dump(4);

    file.close();
}


void DeserializeVariant(rttr::variant& var, const json& j,
    const std::unordered_map<std::string, rttr::variant>& guidTable)
{
    rttr::type t = var.get_type();

    if (t.is_arithmetic())
    {
        if (t == type::get<bool>())
            var = j.get<bool>();
        else if (t == type::get<int>())
            var = j.get<int>();
        else if (t == type::get<float>())
            var = j.get<float>();
        else if (t == type::get<double>())
            var = j.get<double>();
        else if (t == type::get<uint64_t>())
            var = j.get<uint64_t>();
        else if (t == type::get<unsigned int>())
            var = j.get<unsigned int>();
        else if (t == type::get<long long>())
            var = j.get<long long>();
    }
    else if (t == type::get<std::string>())
    {
        var = j.get<std::string>();
    }
    else if (t == type::get<std::wstring>())
    {
        var = Utility::StringHelper::StringToWString(j.get<std::string>());
    }
    else if (t.is_sequential_container())
    {
        auto view = var.create_sequential_view();
        view.set_size(j.size());

        size_t index = 0;
        for (const auto& item : j)
        {
            rttr::variant elemVar = view.get_value(index);
            DeserializeVariant(elemVar, item, guidTable);
            view.set_value(index, elemVar);
            index++;
        }
    }
    else if (t.get_name().to_string().find("ObjPtr") != std::string::npos)
    {
        // GUID 참조는 나중에 처리하기 위해 GUID 문자열을 임시 저장
        // 실제 연결은 2차 패스에서 수행
    }
    else if (t.is_associative_container())
    {
        auto view = var.create_associative_view();
        for (auto& [key, value] : j.items())
        {
            json keyJson = json::parse(key);
            rttr::variant keyVar;
            rttr::variant valueVar;

            DeserializeVariant(keyVar, keyJson, guidTable);
            DeserializeVariant(valueVar, value, guidTable);

            view.insert(keyVar, valueVar);
        }
    }
}

void DeserializeComponent(ObjPtr<Component>& comp, const json& compJson,
    const std::unordered_map<std::string, rttr::variant>& guidTable)
{
    type t = type::get(*comp);

    if (compJson.contains("Props"))
    {
        for (auto& [propName, propValue] : compJson["Props"].items())
        {
            auto prop = t.get_property(propName);
            if (prop.is_valid())
            {
                rttr::variant var = prop.get_value(*comp);
                DeserializeVariant(var, propValue, guidTable);
                prop.set_value(*comp, var);
            }
        }
    }
}

void MMMEngine::SceneSerializer::Deserialize(Scene& scene,const SnapShot& snapShot)
{
    // 파일 읽기
    //std::ifstream file(path, std::ios::binary);
    //if (!file.is_open())
    //    return;

    //std::vector<uint8_t> buffer((std::istreambuf_iterator<char>(file)),
    //    std::istreambuf_iterator<char>());
    //file.close();

    //json snapshot = json::from_msgpack(buffer);

    // GUID -> ObjectPtr 매핑 테이블
    std::unordered_map<std::string, ObjPtr<GameObject>> gameObjectTable;
    std::unordered_map<std::string, ObjPtr<Component>> componentTable;
    std::unordered_map<std::string, rttr::variant> guidTable;

    // 부모 자식 관계 정보 저장용
    struct HierarchyInfo
    {
        std::string childMUID;
        std::string parentMUID;
    };
    std::vector<HierarchyInfo> hierarchyInfos;

    // 1단계: 모든 GameObject와 Component 생성
    for (const auto& goJson : snapShot["GameObjects"])
    {
        std::string goMUID = goJson["MUID"];
        std::string goName = goJson["Name"];

        // GameObject 생성
        auto gameObject = scene.CreateGameObject(goName);
        gameObject->SetMUID(Utility::MUID::Parse(goMUID).value());
        gameObjectTable[goMUID] = gameObject;
        guidTable[goMUID] = gameObject;

        // 부모 정보가 있으면 저장
        if (goJson.contains("ParentMUID"))
        {
            hierarchyInfos.push_back({ goMUID, goJson["ParentMUID"] });
        }

        // Component 생성
        for (const auto& compJson : goJson["Components"])
        {
            if (!compJson.contains("Type") || !compJson.contains("Props")) continue;

            std::string typeName = compJson["Type"];
            std::string compMUID = compJson["Props"]["MUID"];
            std::string typePtrName = "ObjPtr<" + typeName + ">";

            // RTTR을 통해 컴포넌트 타입 생성
            type compType = type::get_by_name(typePtrName);
            if (compType.is_valid())
            {
                rttr::variant compVar = compType.create();

                ObjPtr<Component> comp = nullptr;
                compVar.convert(comp);

                if (comp.IsValid())
                {
                    comp->SetMUID(Utility::MUID::Parse(compMUID).value());
                    //gameObject->AddComponent(comp);
                    {
                        comp->m_gameObject = gameObject;
                        gameObject->RegisterComponent(comp);

                        // todo : UI가져올때
                        //if constexpr (std::is_same<T, Canvas>::value || std::is_base_of<Graphic, T>::value)
                        //{
                        //	gameObject->EnsureRectTransform();
                        //	gameObject->UpdateActiveInHierarchy();
                        //}

                        comp->Initialize();
                    }

                    componentTable[compMUID] = comp;
                    guidTable[compMUID] = comp;

                    // ObjPtr 참조가 아닌 프로퍼티는 바로 복원
                    DeserializeComponent(comp, compJson, guidTable);
                }
            }
        }
    }

    // 2단계: Transform 계층 복원
    for (const auto& info : hierarchyInfos)
    {
        auto child = gameObjectTable[info.childMUID];
        auto parent = gameObjectTable[info.parentMUID];

        if (child.IsValid() && parent.IsValid())
        {
            auto childTransform = child->GetComponent<Transform>();
            auto parentTransform = parent->GetComponent<Transform>();

            if (childTransform.IsValid() && parentTransform.IsValid())
            {
                childTransform->SetParent(parentTransform);
            }
        }
    }

    // 3단계: 컴포넌트의 ObjPtr 참조 복원
    for (const auto& goJson : snapShot["GameObjects"])
    {
        std::string goMUID = goJson["MUID"];
        auto gameObject = gameObjectTable[goMUID];

        for (const auto& compJson : goJson["Components"])
        {
            std::string compMUID = compJson["Props"]["MUID"];
            auto comp = componentTable[compMUID];

            if (!comp.IsValid() || !compJson.contains("Props"))
                continue;

            type t = type::get(*comp);

            for (auto& [propName, propValue] : compJson["Props"].items())
            {
                auto prop = t.get_property(propName);
                if (!prop.is_valid())
                    continue;

                rttr::type propType = prop.get_type();

                // ObjPtr 타입인지 확인
                if (propType.get_name().to_string().find("ObjPtr") != std::string::npos)
                {
                    if (propValue.is_string())
                    {
                        std::string refMUID = propValue.get<std::string>();

                        // GUID 테이블에서 실제 객체 찾기
                        if (guidTable.find(refMUID) != guidTable.end())
                        {
                            prop.set_value(*comp, guidTable[refMUID]);
                        }
                    }
                }
            }
        }
    }
}

