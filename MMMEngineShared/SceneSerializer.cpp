#include "SceneSerializer.h"
#include "GameObject.h"
#include "Component.h"
#include "StringHelper.h"
#include "rttr/type"
#include "Transform.h"
#include "RectTransform.h"
#include "ResourceManager.h"
#include "MissingScriptBehaviour.h"
#include "SerializableEvent.h"
#include "ObjectManager.h"
#include "Graphic.h"
#include "AnimationCurve.h"

#include <fstream>
#include <filesystem>


DEFINE_SINGLETON(MMMEngine::SceneSerializer)

namespace fs = std::filesystem;
using json = nlohmann::json;
using namespace MMMEngine;
using namespace rttr;

std::unordered_map<std::string, rttr::variant> g_objectTable;

static bool IsObjPtrWrapperType(const rttr::type& wrapped_type)
{
    if (!wrapped_type.is_valid() || !wrapped_type.is_pointer())
        return false;

    rttr::type raw = wrapped_type.get_raw_type();
    return (raw == rttr::type::get<MMMEngine::Object>() ||
            raw.is_derived_from(rttr::type::get<MMMEngine::Object>()));
}

static bool IsObjPtrLikeType(const rttr::type& t)
{
    if (t.get_name().to_string().find("ObjPtr") != std::string::npos)
        return true;

    if (t.is_wrapper())
    {
        if (IsObjPtrWrapperType(t.get_wrapped_type()))
            return true;
    }
    return false;
}

static rttr::type ResolveObjPtrInjectType(const rttr::type& target_type)
{
    if (!IsObjPtrLikeType(target_type))
        return rttr::type::get_by_name("");

    if (target_type.get_method("Inject").is_valid())
        return target_type;

    if (target_type.is_wrapper())
    {
        rttr::type wrapped = target_type.get_wrapped_type();
        rttr::type raw = wrapped.get_raw_type();
        if (!raw.is_valid())
            raw = wrapped;
        if (raw.get_method("Inject").is_valid())
            return raw;
    }
    return rttr::type::get_by_name("");
}

json SerializeVariant(const rttr::variant& var);
json SerializeObject(const rttr::instance& obj)
{
    json j;
    type t = obj.get_type();

    for (auto& prop : t.get_properties(
        rttr::filter_item::instance_item |
        rttr::filter_item::public_access |
        rttr::filter_item::non_public_access))
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
    if (!var.is_valid())
        return nullptr;

    rttr::type t = var.get_type();

    if (t.is_wrapper())
    {
        auto args = t.get_template_arguments();
        if (args.begin() != args.end())
        {
            rttr::type innerType = *args.begin();
            rttr::type resourceBase = rttr::type::get<MMMEngine::Resource>();

            if (innerType.is_derived_from(resourceBase) || innerType == resourceBase)
            {
                auto resPtr = var.get_value<std::shared_ptr<MMMEngine::Resource>>();

                if (resPtr && !resPtr->GetFilePath().empty())
                {
                    return MMMEngine::Utility::StringHelper::WStringToString(
                        resPtr->GetFilePath()
                    );
                }

                return nullptr;
            }
        }

        rttr::type wrappedType = t.get_wrapped_type();
        if (wrappedType.is_valid() && !IsObjPtrWrapperType(wrappedType))
        {
            rttr::variant unwrapped = var.extract_wrapped_value();
            if (unwrapped.is_valid() && unwrapped.get_type() != t)
                return SerializeVariant(unwrapped);
        }
        // ObjPtr wrapper는 아래 ObjPtr 분기에서 처리하도록 fallthrough
    }

    if (t.is_enumeration())
    {
        rttr::enumeration enumType = t.get_enumeration();
        std::string enumName = enumType.value_to_name(var).to_string();

        json enumJson;
        enumJson["EnumType"] = t.get_name().to_string();
        enumJson["EnumValue"] = enumName;
        return enumJson;
    }

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

    if (IsObjPtrLikeType(t))
    {
        MMMEngine::Object* obj = nullptr;
        if (var.convert(obj) && obj != nullptr)
            return obj->GetMUID().ToString();
        return nullptr;
    }

    // SerializableEvent / SerializableEventT<float> -> 배열 { TargetMUID, MessageName }
    if (t == type::get<MMMEngine::SerializableEvent>())
    {
        json arr = json::array();
        const auto& ev = var.get_value<MMMEngine::SerializableEvent>();
        for (const auto& call : ev.GetCalls())
        {
            arr.push_back({ {"TargetMUID", call.targetMUID}, {"MessageName", call.messageName} });
        }
        return arr;
    }
    if (t == type::get<MMMEngine::SerializableEventT<float>>())
    {
        json arr = json::array();
        const auto& ev = var.get_value<MMMEngine::SerializableEventT<float>>();
        for (const auto& call : ev.GetCalls())
        {
            arr.push_back({ {"TargetMUID", call.targetMUID}, {"MessageName", call.messageName} });
        }
        return arr;
    }
    if (t == type::get<MMMEngine::SerializableEventT<bool>>())
    {
        json arr = json::array();
        const auto& ev = var.get_value<MMMEngine::SerializableEventT<bool>>();
        for (const auto& call : ev.GetCalls())
        {
            arr.push_back({ {"TargetMUID", call.targetMUID}, {"MessageName", call.messageName} });
        }
        return arr;
    }
    if (t == type::get<MMMEngine::SerializableEventT<int>>())
    {
        json arr = json::array();
        const auto& ev = var.get_value<MMMEngine::SerializableEventT<int>>();
        for (const auto& call : ev.GetCalls())
        {
            arr.push_back({ {"TargetMUID", call.targetMUID}, {"MessageName", call.messageName} });
        }
        return arr;
    }
    if (t == type::get<MMMEngine::SerializableEventT<std::string>>())
    {
        json arr = json::array();
        const auto& ev = var.get_value<MMMEngine::SerializableEventT<std::string>>();
        for (const auto& call : ev.GetCalls())
        {
            arr.push_back({ {"TargetMUID", call.targetMUID}, {"MessageName", call.messageName} });
        }
        return arr;
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

    ObjPtr<MissingScriptBehaviour> missing;
    {
        try { missing = comp.Cast<MissingScriptBehaviour>(); }
        catch (...) { /* ignore */ }
    }

    if (missing.IsValid())
    {
        const std::string& originalType = missing->GetOriginalTypeName();
        compJson["Type"] = originalType.empty() ? std::string("MissingScriptBehaviour") : originalType;

        // props를 그대로 저장 (msgpack 보관본이 있으면 그걸 사용)
        if (missing->HasOriginalProps())
        {
            json props = json::from_msgpack(missing->GetOriginalPropsMsgPack());
            compJson["Props"] = props;
        }
        else
        {
            // 보관본이 없으면, 최소한 MUID라도 저장되게 fallback
            compJson["Props"] = json::object();
            compJson["Props"]["MUID"] = comp->GetMUID().ToString(); // GetMUID가 Component에 있다고 가정
        }

        return compJson;
    }

    for (auto& prop : type.get_properties(
        rttr::filter_item::instance_item |
        rttr::filter_item::public_access |
        rttr::filter_item::non_public_access))
    {
        auto string = prop.get_name().to_string();
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
    snapshot["Name"] = scene.GetName();
    //Utility::StringHelper::WStringToString(Utility::StringHelper::ExtractFileName(path));

    json goArray = json::array();

    for (auto& goPtr : scene.m_gameObjects)
    {
        if (!goPtr.IsValid())
            continue;

        json goJson;
        goJson["Name"] = goPtr->GetName();
        goJson["MUID"] = goPtr->GetMUID().ToString();
        goJson["Layer"] = goPtr->GetLayer();
        goJson["Tag"] = goPtr->GetTag();
        goJson["Active"] = goPtr->IsActiveSelf();

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

    fs::path p(path);
    if (p.has_parent_path() && !fs::exists(p.parent_path())) {
        fs::create_directories(p.parent_path());
    }

    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("파일을 열 수 없습니다: " + Utility::StringHelper::WStringToString(path));
    }

    file.write(reinterpret_cast<const char*>(v.data()), v.size());
    file.close();
}

static bool IsMissingScriptTargetVariant(const rttr::variant& v)
{
    MMMEngine::Object* o = nullptr;
    if (!v.convert(o) || !o)
        return false;

    rttr::type ot = rttr::type::get(*o);
    return ot.is_derived_from(rttr::type::get<MMMEngine::MissingScriptBehaviour>());
}

void DeserializeVariant(rttr::variant& target, const json& j, type target_type);

void DeserializeObject(rttr::instance& obj, const json& j)
{
    type t = obj.get_derived_type();

    for (auto& prop : t.get_properties(
        rttr::filter_item::instance_item |
        rttr::filter_item::public_access |
        rttr::filter_item::non_public_access))
    {
        if (prop.is_readonly())
            continue;

        std::string propName = prop.get_name().to_string();
        if (!j.contains(propName))
            continue;

        rttr::variant currentValue = prop.get_value(obj);
        DeserializeVariant(currentValue, j[propName], prop.get_type());
        prop.set_value(obj, currentValue);
    }
}

void DeserializeVariant(rttr::variant& target, const json& j, type target_type)
{
    if (j.is_null())
    {
        target = rttr::variant();
        return;
    }

    if (target_type.is_wrapper())
    {
        auto args = target_type.get_template_arguments();
        if (args.begin() != args.end())
        {
            rttr::type innerType = *args.begin();
            if (innerType.is_derived_from(rttr::type::get<Resource>()))
            {
                std::string pathStr = j.get<std::string>();
                std::wstring filePath = Utility::StringHelper::StringToWString(pathStr);

                rttr::variant loadedResource = ResourceManager::Get().Load(
                    innerType, filePath);

                if (loadedResource.convert(target.get_type()))
                {
                    target = loadedResource;
                }
                return;
            }
        }

        rttr::type wrapped = target_type.get_wrapped_type();
        if (wrapped.is_valid())
        {
            bool isObjPtrWrapper = false;
            if (wrapped.is_pointer())
            {
                rttr::type raw = wrapped.get_raw_type();
                if (raw == type::get<MMMEngine::Object>() || raw.is_derived_from(type::get<MMMEngine::Object>()))
                    isObjPtrWrapper = true;
            }

            // ObjPtr는 아래 ObjPtr 분기에서 처리
            if (!isObjPtrWrapper)
            {
                rttr::type raw_wrapped = wrapped.get_raw_type();
                if (!raw_wrapped.is_valid())
                    raw_wrapped = wrapped;
                rttr::variant unwrapped = target.extract_wrapped_value();
                if (!unwrapped.is_valid() || unwrapped.get_type() != raw_wrapped)
                    unwrapped = raw_wrapped.create();
                DeserializeVariant(unwrapped, j, raw_wrapped);
                target = unwrapped;
                return;
            }
        }
    }

    if (target_type.is_enumeration())
    {
        if (j.contains("EnumType") && j.contains("EnumValue"))
        {
            std::string enumTypeName = j["EnumType"].get<std::string>();
            std::string enumValueName = j["EnumValue"].get<std::string>();

            rttr::enumeration enumType = target_type.get_enumeration();
            rttr::variant enumValue = enumType.name_to_value(enumValueName);

            if (enumValue.is_valid())
            {
                target = enumValue;
            }
        }
        return;
    }

    if (target_type.is_arithmetic())
    {
        if (target_type == type::get<bool>()) target = j.get<bool>();
        else if (target_type == type::get<int>()) target = j.get<int>();
        else if (target_type == type::get<unsigned int>()) target = j.get<unsigned int>();
        else if (target_type == type::get<long long>()) target = j.get<long long>();
        else if (target_type == type::get<uint64_t>()) target = j.get<uint64_t>();
        else if (target_type == type::get<float>()) target = j.get<float>();
        else if (target_type == type::get<double>()) target = j.get<double>();
        return;
    }

    if (target_type == type::get<MMMEngine::Utility::MUID>())
    {
        std::string muidStr = j.get<std::string>();
        if (auto parsed = MMMEngine::Utility::MUID::Parse(muidStr); parsed.has_value())
            target = parsed.value();
        else
            target = MMMEngine::Utility::MUID::Empty();
        return;
    }

    if (target_type == type::get<std::string>())
    {
        target = j.get<std::string>();
        return;
    }



    // SerializableEvent / SerializableEventT<float> <- 배열 { TargetMUID, MessageName }
    if (target_type == type::get<MMMEngine::SerializableEvent>())
    {
        std::vector<MMMEngine::PersistentCall> calls;
        if (j.is_array())
        {
            for (const auto& item : j)
            {
                std::string muid = item.contains("TargetMUID") ? item["TargetMUID"].get<std::string>() : "";
                std::string name = item.contains("MessageName") ? item["MessageName"].get<std::string>() : "";
                calls.emplace_back(std::move(muid), std::move(name));
            }
        }
        MMMEngine::SerializableEvent ev;
        ev.SetCalls(std::move(calls));
        target = ev;
        return;
    }
    if (target_type == type::get<MMMEngine::SerializableEventT<float>>())
    {
        std::vector<MMMEngine::PersistentCall> calls;
        if (j.is_array())
        {
            for (const auto& item : j)
            {
                std::string muid = item.contains("TargetMUID") ? item["TargetMUID"].get<std::string>() : "";
                std::string name = item.contains("MessageName") ? item["MessageName"].get<std::string>() : "";
                calls.emplace_back(std::move(muid), std::move(name));
            }
        }
        MMMEngine::SerializableEventT<float> ev;
        ev.SetCalls(std::move(calls));
        target = ev;
        return;
    }
    if (target_type == type::get<MMMEngine::SerializableEventT<bool>>())
    {
        std::vector<MMMEngine::PersistentCall> calls;
        if (j.is_array())
        {
            for (const auto& item : j)
            {
                std::string muid = item.contains("TargetMUID") ? item["TargetMUID"].get<std::string>() : "";
                std::string name = item.contains("MessageName") ? item["MessageName"].get<std::string>() : "";
                calls.emplace_back(std::move(muid), std::move(name));
            }
        }
        MMMEngine::SerializableEventT<bool> ev;
        ev.SetCalls(std::move(calls));
        target = ev;
        return;
    }
    if (target_type == type::get<MMMEngine::SerializableEventT<int>>())
    {
        std::vector<MMMEngine::PersistentCall> calls;
        if (j.is_array())
        {
            for (const auto& item : j)
            {
                std::string muid = item.contains("TargetMUID") ? item["TargetMUID"].get<std::string>() : "";
                std::string name = item.contains("MessageName") ? item["MessageName"].get<std::string>() : "";
                calls.emplace_back(std::move(muid), std::move(name));
            }
        }
        MMMEngine::SerializableEventT<int> ev;
        ev.SetCalls(std::move(calls));
        target = ev;
        return;
    }
    if (target_type == type::get<MMMEngine::SerializableEventT<std::string>>())
    {
        std::vector<MMMEngine::PersistentCall> calls;
        if (j.is_array())
        {
            for (const auto& item : j)
            {
                std::string muid = item.contains("TargetMUID") ? item["TargetMUID"].get<std::string>() : "";
                std::string name = item.contains("MessageName") ? item["MessageName"].get<std::string>() : "";
                calls.emplace_back(std::move(muid), std::move(name));
            }
        }
        MMMEngine::SerializableEventT<std::string> ev;
        ev.SetCalls(std::move(calls));
        target = ev;
        return;
    }

    if (target_type.is_sequential_container())
    {
        rttr::type container_type = target_type.get_raw_type();
        if (!container_type.is_valid())
            container_type = target_type;

        if (!target.is_valid() || target.get_type() != container_type)
            target = container_type.create();

        auto view = target.create_sequential_view();
        view.clear();

        auto args = container_type.get_template_arguments();
        auto it = args.begin();
        if (it == args.end())
            return;

        rttr::type value_type = *it;

        for (const auto& item : j)
        {
            rttr::variant element = value_type.create();
            DeserializeVariant(element, item, value_type);
            view.insert(view.end(), element);
        }
        return;
    }

    if (target_type.is_associative_container())
    {
        rttr::type container_type = target_type.get_raw_type();
        if (!container_type.is_valid())
            container_type = target_type;

        if (!target.is_valid() || target.get_type() != container_type)
            target = container_type.create();

        auto view = target.create_associative_view();
        view.clear();

        auto args = container_type.get_template_arguments();
        auto it = args.begin();
        if (it == args.end())
            return;

        rttr::type key_type = *it;
        ++it;
        if (it == args.end())
            return;

        rttr::type value_type = *it;

        for (auto& [key, value] : j.items())
        {
            rttr::variant k = key_type.create();
            rttr::variant v = value_type.create();

            json keyJson = json::parse(key);
            rttr::instance kInst(k);
            rttr::instance vInst(v);
            DeserializeObject(kInst, keyJson);
            DeserializeObject(vInst, value);

            view.insert(k, v);
        }
        return;
    }

    if (j.is_array())
    {
        rttr::property keyframesProp = target_type.get_property("keyframes");
        if (keyframesProp.is_valid() && keyframesProp.get_type().is_sequential_container())
        {
            rttr::type raw_type = target_type.get_raw_type();
            if (!raw_type.is_valid())
                raw_type = target_type;

            if (!target.is_valid() || target.get_type() != raw_type)
                target = raw_type.create();
            if (!target.is_valid())
                return;

            rttr::variant keyframes = keyframesProp.get_value(target);
            DeserializeVariant(keyframes, j, keyframesProp.get_type());
            keyframesProp.set_value(target, keyframes);
            return;
        }
    }

    // ObjPtr<T> 타입 처리 (ObjPtr wrapper 포함)
    rttr::type inject_type = ResolveObjPtrInjectType(target_type);
    if (inject_type.is_valid())
    {
        auto inject = inject_type.get_method("Inject");
        if (!inject.is_valid())
        {
            // Inject가 등록 안 된 ObjPtr면 복원 불가
            target = rttr::variant();
            return;
        }

        // 1) null이면: "null handle" 규약으로 Inject
        if (j.is_null())
        {
            ObjPtr<Object> nullObj;               // default: raw=null, ptrID=UINT32_MAX 규약이라면 OK
            const ObjPtrBase& nullRef = nullObj;

            rttr::variant okv = inject.invoke(target, nullRef);
            return;
        }

        // 2) MUID로 테이블 lookup (문자열이 아니면 null 처리 — 구형 씬에서 {} 등으로 저장된 경우 방어)
        if (!j.is_string())
        {
            ObjPtr<Object> nullObj;
            const ObjPtrBase& nullRef = nullObj;
            inject.invoke(target, nullRef);
            return;
        }
        std::string muidStr = j.get<std::string>();
        auto it = g_objectTable.find(muidStr);
        if (it == g_objectTable.end() || IsMissingScriptTargetVariant(it->second))
        {
            ObjPtr<Object> nullObj;
            const ObjPtrBase& nullRef = nullObj;
            inject.invoke(target, nullRef);
            return;
        }

        // 3) table에는 ObjPtr<Object>만 저장해놨으니 그대로 꺼내서 Inject
        rttr::variant src = it->second;

        // 안전하게: ObjPtr<Object>로 꺼내고(복사), 그 복사의 baseRef를 Inject에 넘김
        if (src.is_type<ObjPtr<Object>>())
        {
            ObjPtr<Object> base = src.get_value<ObjPtr<Object>>();
            const ObjPtrBase& baseRef = base;

            rttr::variant okv = inject.invoke(target, baseRef);
            return;
        }

        // fallback: 타입이 예상과 다르면 null 처리
        ObjPtr<Object> nullObj;
        const ObjPtrBase& nullRef = nullObj;
        inject.invoke(target, nullRef);
        return;
    }
    // 사용자 정의 객체
    if (!target.is_valid() || target.get_type() != target_type)
    {
        target = target_type.create();
    }

    rttr::instance inst(target);
    DeserializeObject(inst, j);
}

struct PendingComponentProps
{
    ObjPtr<Component> comp;
    const json* props = nullptr;
};

ObjPtr<Component> CreateComponentForDeserialize(const json& compJson, ObjPtr<GameObject> obj, bool& outIsMissing)
{
    outIsMissing = false;

    if (!compJson.contains("Type"))
        return {};

    std::string typeName = compJson["Type"].get<std::string>();
    type compType = type::get_by_name(typeName);

    const json* propsPtr = compJson.contains("Props") ? &compJson["Props"] : nullptr;

    if (!compType.is_valid())
    {
        compType = rttr::type::get<MissingScriptBehaviour>();

        auto compVar = obj->AddComponent(compType);
        if (!compVar.IsValid())
            return {};

        if (propsPtr && propsPtr->contains("MUID"))
        {
            std::string muid = (*propsPtr)["MUID"].get<std::string>();
            g_objectTable[muid] = ObjPtr<Object>(compVar);
        }

        ObjPtr<MissingScriptBehaviour> missing = compVar.Cast<MissingScriptBehaviour>();
        if (missing.IsValid())
        {
            missing->SetOriginalTypeName(typeName);
            if (propsPtr)
            {
                std::vector<uint8_t> packed = json::to_msgpack(*propsPtr);
                missing->SetOriginalPropsMsgPack(std::move(packed));
            }
        }

        outIsMissing = true;
        return compVar;
    }

    auto comp = obj->AddComponent(compType);
    if (!comp.IsValid())
        return {};

    if (propsPtr && propsPtr->contains("MUID"))
    {
        std::string muid = (*propsPtr)["MUID"].get<std::string>();
        g_objectTable[muid] = ObjPtr<Object>(comp);
    }

    return comp;
}

void DeserializeTransform(Transform& tr, const json& j, const type& t)
{
    for (auto& prop : t.get_properties(
        rttr::filter_item::instance_item |
        rttr::filter_item::public_access |
        rttr::filter_item::non_public_access))
    {
        if (prop.is_readonly())
            continue;

        std::string name = prop.get_name().to_string();
        if (name == "Parent" || name == "m_parent" || name == "MUID" || name == "m_muid")
            continue;

        if (!j.contains(name))
            continue;

        rttr::variant v = prop.get_value(tr);
        DeserializeVariant(v, j[name], prop.get_type());
        prop.set_value(tr, v);
    }
}

struct TransformCompInfo
{
    const json* comp = nullptr;
    bool isRect = false;
};

static TransformCompInfo FindTransformComp(const json& components)
{
    TransformCompInfo info;

    for (const auto& c : components)
    {
        if (!c.contains("Type")) continue;
        std::string t = c["Type"].get<std::string>();
        if (t == "RectTransform")
        {
            info.comp = &c;
            info.isRect = true;
            return info;
        }
    }

    for (const auto& c : components)
    {
        if (!c.contains("Type")) continue;
        std::string t = c["Type"].get<std::string>();
        if (t == "Transform")
        {
            info.comp = &c;
            info.isRect = false;
            return info;
        }
    }

    return info;
}

void MMMEngine::SceneSerializer::Deserialize(Scene& scene, const SnapShot& snapshot)
{
    g_objectTable.clear();
    std::unordered_map<std::string, std::string> pendingParent; // childTrMUID -> parentTrMUID

    // Scene MUID
    if (auto parsed = Utility::MUID::Parse(snapshot["MUID"].get<std::string>()); parsed.has_value())
        scene.SetMUID(parsed.value());

    // Scene Name
    scene.SetName(snapshot["Name"].get<std::string>());

    const json& gameObjects = snapshot["GameObjects"];

    // 1-pass: GO + Transform(혹은 RectTransform인 경우 곧바로 EnsureTransform) MUID/값 복원 + 테이블 등록
    for (const auto& goJson : gameObjects)
    {
        std::string goName = goJson["Name"].get<std::string>();
        std::string goMUID = goJson["MUID"].get<std::string>();
        uint32_t goLayer = goJson["Layer"].get<uint32_t>();
        std::string goTag = goJson["Tag"].get<std::string>();
        bool active = true;
        if (goJson.contains("Active"))
        {
            active = goJson["Active"].get<bool>();
        }

        ObjPtr<GameObject> go = scene.CreateGameObject(goName);
        scene.RegisterGameObject(go);
        go->SetName(goName);
        go->SetLayer(goLayer);
        go->SetTag(goTag);
        go->SetActive(active);

        if (auto parsedGo = Utility::MUID::Parse(goMUID); parsedGo.has_value())
            go->SetMUID(parsedGo.value());

        g_objectTable[goMUID] = ObjPtr<Object>(go);

        // todo : 여기서 RectTransform도 같이 찾기 -> 분기 생성
        // Transform json 찾기
        const json& components = goJson["Components"];
        TransformCompInfo trCompInfo = FindTransformComp(components);
        if (!trCompInfo.comp || !trCompInfo.comp->contains("Props"))
            continue; // 또는 throw

        const json& trProps = (*trCompInfo.comp)["Props"];

        if (trCompInfo.isRect)
            go->EnsureRectTransform();

        // 기존 Transform 가져오기
        auto tr = go->GetTransform();

        // Transform MUID는 Props["MUID"]
        std::string trMUID = trProps["MUID"].get<std::string>();
        if (auto parsedTr = Utility::MUID::Parse(trMUID); parsedTr.has_value())
            tr->SetMUID(parsedTr.value());

        g_objectTable[trMUID] = ObjPtr<Object>(tr);

        // Transform 값 복원 (Parent/MUID는 스킵)
        auto trType = trCompInfo.isRect ? type::get<RectTransform>() : type::get<Transform>();
        DeserializeTransform(*tr, trProps, trType);

        // Parent는 나중에
        if (trProps.contains("Parent") && !trProps["Parent"].is_null())
            pendingParent[trMUID] = trProps["Parent"].get<std::string>();
    }

    std::vector<PendingComponentProps> pendingComponentProps;

    // 2-pass: 일반 컴포넌트 생성 + MUID 등록 (Transform은 제외)
    //         주의: Collider는 Initialize 시 자동으로 RigidBody를 만들 수 있으므로
    //         RigidBody를 먼저 생성해 중복/파괴를 방지한다.
    for (const auto& goJson : gameObjects)
    {
        std::string goMUID = goJson["MUID"].get<std::string>();
        auto itGo = g_objectTable.find(goMUID);
        if (itGo == g_objectTable.end()) continue;

        ObjPtr<GameObject> go = itGo->second.get_value<ObjPtr<GameObject>>();
        const json& components = goJson["Components"];

        // 2-1) RigidBodyComponent 선 생성
        for (const auto& compJson : components)
        {
            if (!compJson.contains("Type"))
                continue;

            std::string typeName = compJson["Type"].get<std::string>();
            if (typeName != "RigidBodyComponent")
                continue;

            bool isMissing = false;
            ObjPtr<Component> comp = CreateComponentForDeserialize(compJson, go, isMissing);
            if (!comp.IsValid())
                continue;

            if (!isMissing && compJson.contains("Props"))
            {
                PendingComponentProps pending;
                pending.comp = comp;
                pending.props = &compJson["Props"];
                pendingComponentProps.push_back(std::move(pending));
            }
        }

        // 2-2) 나머지 컴포넌트 생성
        for (const auto& compJson : components)
        {
            if (!compJson.contains("Type"))
                continue;

            std::string typeName = compJson["Type"].get<std::string>();
            if (typeName == "Transform" || typeName == "RectTransform" || typeName == "RigidBodyComponent")
                continue;

            bool isMissing = false;
            ObjPtr<Component> comp = CreateComponentForDeserialize(compJson, go, isMissing);
            if (!comp.IsValid())
                continue;

            if (!isMissing && compJson.contains("Props"))
            {
                PendingComponentProps pending;
                pending.comp = comp;
                pending.props = &compJson["Props"];
                pendingComponentProps.push_back(std::move(pending));
            }
        }
    }

    // 3-pass: 일반 컴포넌트 프로퍼티 복원 (모든 ObjPtr이 테이블에 등록된 뒤)
    for (auto& pending : pendingComponentProps)
    {
        if (!pending.comp.IsValid() || pending.comp->IsDestroyed())
            continue;

        if (!pending.props)
            continue;

        rttr::instance inst(*pending.comp);
        DeserializeObject(inst, *pending.props);
    }

    // 4-pass: Parent 연결 (Transform MUID 기준)
    for (auto& [childTrMUID, parentTrMUID] : pendingParent)
    {
        auto itChild = g_objectTable.find(childTrMUID);
        auto itParent = g_objectTable.find(parentTrMUID);

        if (itChild == g_objectTable.end() || itParent == g_objectTable.end())
            continue; // 또는 로그

		auto childTr = itChild->second.get_value<ObjPtr<Transform>>();
		auto parentTr = itParent->second.get_value<ObjPtr<Transform>>();
		childTr->SetParent(parentTr, false);
	}

	// 부모 체인이 완성된 뒤 UI Graphic의 Canvas 참조 갱신
	for (auto& goPtr : scene.m_gameObjects)
	{
		if (!goPtr.IsValid())
			continue;

		for (auto& comp : goPtr->GetAllComponents())
		{
			if (!comp.IsValid() || comp->IsDestroyed())
				continue;

			if (auto graphic = comp.Cast<Graphic>(); graphic.IsValid())
				graphic->RefreshCanvasNow();
		}
	}

    // SerializableEvent 리졸버는 ObjectManager의 MUID 테이블을 사용한다.
    SerializableEvent::SetResolver([](const Utility::MUID& muid) { return ObjectManager::Get().GetObjectByMUID(muid); });
    SerializableEventT<float>::SetResolver([](const Utility::MUID& muid) { return ObjectManager::Get().GetObjectByMUID(muid); });
    SerializableEventT<bool>::SetResolver([](const Utility::MUID& muid) { return ObjectManager::Get().GetObjectByMUID(muid); });
    SerializableEventT<int>::SetResolver([](const Utility::MUID& muid) { return ObjectManager::Get().GetObjectByMUID(muid); });
    SerializableEventT<std::string>::SetResolver([](const Utility::MUID& muid) { return ObjectManager::Get().GetObjectByMUID(muid); });

    g_objectTable.clear();
}

void MMMEngine::SceneSerializer::SerializeToMemory(const Scene& scene, SnapShot& snapshot, bool makeDefaultObjects)
{
    auto sceneMUID = scene.GetMUID().IsEmpty() ? Utility::MUID::NewMUID() : scene.GetMUID();

    snapshot["MUID"] = sceneMUID.ToString();
    snapshot["Name"] = scene.GetName();
    //Utility::StringHelper::WStringToString(Utility::StringHelper::ExtractFileName(path));

    json goArray = json::array();

    for (auto& goPtr : scene.m_gameObjects)
    {
        if (!goPtr.IsValid())
            continue;

        json goJson;
        goJson["Name"] = goPtr->GetName();
        goJson["MUID"] = goPtr->GetMUID().ToString();
        goJson["Layer"] = goPtr->GetLayer();
        goJson["Tag"] = goPtr->GetTag();
        goJson["Active"] = goPtr->IsActiveSelf();

        json compArray = json::array();
        for (auto& comp : goPtr->GetAllComponents()) // 컴포넌트 리스트 가정
        {
            compArray.push_back(SerializeComponent(comp));
        }
        goJson["Components"] = compArray;

        goArray.push_back(goJson);
    }

    if (makeDefaultObjects)
    {
        // 카메라 생성
        json goJson;
        goJson["Name"] = "MainCamera";
        goJson["MUID"] = Utility::MUID::NewMUID().ToString();
        goJson["Layer"] = 0;
        goJson["Tag"] = "MainCamera";
        goJson["Active"] = true;

        json compJson;
        json compArray = json::array();
        compJson["Type"] = "Transform";
        compJson["Props"]["Position"]["z"] = 5.0f;
        compJson["Props"]["Rotation"]["y"] = -1.0f;
        compJson["Props"]["Rotation"]["w"] = 0.0f;
        compJson["Props"]["MUID"] = Utility::MUID::NewMUID().ToString();
        compJson["Props"]["Parent"] = nullptr;

        compArray.push_back(compJson);

        compJson.clear();
        compJson["Type"] = "Camera";
        compJson["Props"]["FOV"] = 60;
        compJson["Props"]["Near"] = 0.1f;
        compJson["Props"]["Far"] = 1000.0f;
        compJson["Props"]["AspectRatio"] = 16.0f / 9.0f;
        compJson["Props"]["Enabled"] = true;
        compJson["Props"]["MUID"] = Utility::MUID::NewMUID().ToString();

        compArray.push_back(compJson);

        compJson.clear();
        compJson["Type"] = "AudioListener";
        compJson["Props"]["MUID"] = Utility::MUID::NewMUID().ToString();

        compArray.push_back(compJson);

        goJson["Components"] = compArray;
        goArray.push_back(goJson);

        // Directional Light 생성
        goJson.clear();
        compJson.clear();
        compArray.clear();

		goJson["Name"] = "Directional Light";
		goJson["MUID"] = Utility::MUID::NewMUID().ToString();
		goJson["Layer"] = 0;
		goJson["Tag"] = "";
        goJson["Active"] = true;
        
        compJson["Type"] = "Transform";
        compJson["Props"]["Rotation"]["x"] = -0.7071f;
        compJson["Props"]["Rotation"]["y"] = 0.0f;
        compJson["Props"]["Rotation"]["z"] = 0.0f;
        compJson["Props"]["Rotation"]["w"] = 0.7071f;
        compJson["Props"]["MUID"] = Utility::MUID::NewMUID().ToString();
        compJson["Props"]["Parent"] = nullptr;
        compArray.push_back(compJson);
        compJson.clear();
        compJson["Type"] = "Light";
        compJson["Props"]["LightType"] = 1; // Directional
        compJson["Props"]["Color"]["r"] = 1.0f;
        compJson["Props"]["Color"]["g"] = 1.0f;
        compJson["Props"]["Color"]["b"] = 1.0f;
        compJson["Props"]["Intensity"] = 1.0f;
        compJson["Props"]["Enabled"] = true;
        compJson["Props"]["MUID"] = Utility::MUID::NewMUID().ToString();
        compArray.push_back(compJson);
        goJson["Components"] = compArray;
		goArray.push_back(goJson);
    }

    snapshot["GameObjects"] = goArray;
}

/// <summary>
/// 최적화된 단일경로용 바이너리 파일을 만들어줍니다.
/// </summary>
/// <param name="scenes"></param>
/// <param name="rootPath"></param>
void MMMEngine::SceneSerializer::ExtractScenesList(const std::vector<Scene*>& scenes,
    const std::wstring& rootPath)
{
    namespace fs = std::filesystem;

    nlohmann::json sceneListJson = nlohmann::json::array();

    fs::path root(rootPath);

    for (size_t i = 0; i < scenes.size(); ++i)
    {
        Scene* scene = scenes[i];
        if (!scene) continue;

        // "custom1.scene"
        const std::string sceneFileName = scene->GetName() + ".scene";

        nlohmann::json sceneEntry;
        sceneEntry["index"] = i;
        sceneEntry["filepath"] = sceneFileName; // 리스트에는 상대경로(파일명)만 저장
        sceneListJson.push_back(sceneEntry);
    }

    // rootpath/sceneList.bin
    fs::path listFullPath = root / L"sceneList.bin";

    std::ofstream outFile(listFullPath, std::ios::binary);
    if (outFile)
    {
        std::vector<uint8_t> msgpack = nlohmann::json::to_msgpack(sceneListJson);
        outFile.write(reinterpret_cast<const char*>(msgpack.data()),
            static_cast<std::streamsize>(msgpack.size()));
    }
}
