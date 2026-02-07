#include "ObjectSerializer.h"
#include "GameObject.h"
#include "Component.h"
#include "MissingScriptBehaviour.h"
#include "Prefab.h"
#include "ObjectManager.h"
#include "Transform.h"
#include "RectTransform.h"
#include "Graphic.h"
#include "SceneManager.h"
#include "ResourceManager.h"
#include "SerializableEvent.h"
#include "StringHelper.h"
#include "AnimationCurve.h"
#include "BehaviourManager.h"
#include "GlobalRegistry.h"
#include "json/json.hpp"
#include "rttr/registration"
#include "rttr/type"

#include <fstream>
#include <unordered_map>
#include <vector>

DEFINE_SINGLETON(MMMEngine::ObjectSerializer)

namespace MMMEngine
{
    namespace
    {
        using json = nlohmann::json;
        using namespace rttr;

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

        void CollectHierarchy(const ObjPtr<GameObject>& root, std::vector<ObjPtr<GameObject>>& out)
        {
            if (!root.IsValid() || root->IsDestroyed())
                return;

            std::vector<ObjPtr<GameObject>> stack;
            stack.push_back(root);

            while (!stack.empty())
            {
                ObjPtr<GameObject> current = stack.back();
                stack.pop_back();

                if (!current.IsValid() || current->IsDestroyed())
                    continue;

                out.push_back(current);

                auto tr = current->GetTransform();
                if (!tr.IsValid())
                    continue;

                const size_t childCount = tr->GetChildCount();
                for (size_t i = childCount; i-- > 0;)
                {
                    auto childTr = tr->GetChild(i);
                    if (!childTr.IsValid())
                        continue;

                    auto childGo = childTr->GetGameObject();
                    if (childGo.IsValid() && !childGo->IsDestroyed())
                        stack.push_back(childGo);
                }
            }
        }

        // ---- Clone (Instantiate) helpers ----
        struct CloneContext
        {
            std::unordered_map<const Object*, ObjPtr<Object>> objectMap;
            std::vector<std::pair<ObjPtr<Component>, ObjPtr<Component>>> componentPairs;
            std::vector<std::pair<ObjPtr<Transform>, ObjPtr<Transform>>> transformPairs;
        };

        ObjPtr<Transform> FindMappedTransform(const CloneContext& ctx, const ObjPtr<Transform>& original)
        {
            if (!original.IsValid())
                return ObjPtr<Transform>();

            auto it = ctx.objectMap.find(original.operator->());
            if (it == ctx.objectMap.end())
                return ObjPtr<Transform>();

            return it->second.Cast<Transform>();
        }

        rttr::variant CloneVariant(const rttr::variant& src, const rttr::type& targetType, const CloneContext& ctx);

        void CloneObject(rttr::instance srcObj, rttr::instance dstObj, const CloneContext& ctx)
        {
            type t = srcObj.get_derived_type();
            const bool isObjectDerived =
                t.is_derived_from(type::get<Object>()) ||
                t == type::get<Object>();

            for (auto& prop : t.get_properties(
                rttr::filter_item::instance_item |
                rttr::filter_item::public_access |
                rttr::filter_item::non_public_access))
            {
                if (prop.is_readonly())
                    continue;

                const std::string propName = prop.get_name().to_string();

                if (isObjectDerived && propName == "MUID")
                    continue;

                if (t == type::get<Transform>() &&
                    (propName == "Parent" || propName == "m_parent"))
                    continue;

                rttr::variant value = prop.get_value(srcObj);
                rttr::variant cloned = CloneVariant(value, prop.get_type(), ctx);
                prop.set_value(dstObj, cloned);
            }
        }

        rttr::variant CloneVariant(const rttr::variant& src, const rttr::type& targetType, const CloneContext& ctx)
        {
            if (!src.is_valid())
                return rttr::variant();

            if (targetType.is_enumeration())
                return src;

            if (targetType.is_arithmetic())
            {
                if (src.get_type() == targetType)
                    return src;

                rttr::variant converted = src;
                if (converted.convert(targetType))
                    return converted;

                return src;
            }

            if (targetType == type::get<std::string>() ||
                targetType == type::get<MMMEngine::Utility::MUID>())
            {
                return src;
            }

            if (targetType.get_name().to_string().find("ObjPtr") != std::string::npos)
            {
                auto inject = targetType.get_method("Inject");
                rttr::variant target = targetType.create();
                if (!inject.is_valid() || !target.is_valid())
                    return rttr::variant();

                Object* raw = nullptr;
                if (!src.convert(raw) || raw == nullptr || raw->IsDestroyed())
                {
                    ObjPtr<Object> nullObj;
                    const ObjPtrBase& nullRef = nullObj;
                    inject.invoke(target, nullRef);
                    return target;
                }

                auto it = ctx.objectMap.find(raw);
                ObjPtr<Object> mapped = (it != ctx.objectMap.end())
                    ? it->second
                    : ObjectManager::Get().GetPtrFromRaw<Object>(raw);

                if (!mapped.IsValid())
                {
                    ObjPtr<Object> nullObj;
                    const ObjPtrBase& nullRef = nullObj;
                    inject.invoke(target, nullRef);
                    return target;
                }

                const ObjPtrBase& baseRef = mapped;
                inject.invoke(target, baseRef);
                return target;
            }

            if (targetType.is_sequential_container())
            {
                rttr::variant target = targetType.create();
                if (!target.is_valid())
                    return src;

                auto view = target.create_sequential_view();
                view.clear();

                auto args = targetType.get_wrapped_type().get_template_arguments();
                auto it = args.begin();
                if (it == args.end())
                    return target;

                rttr::type valueType = *it;

                auto srcView = src.create_sequential_view();
                for (const auto& item : srcView)
                {
                    rttr::variant cloned = CloneVariant(item, valueType, ctx);
                    view.insert(view.end(), cloned);
                }

                return target;
            }

            if (targetType.is_associative_container())
            {
                rttr::variant target = targetType.create();
                if (!target.is_valid())
                    return src;

                auto view = target.create_associative_view();
                view.clear();

                auto args = targetType.get_wrapped_type().get_template_arguments();
                auto it = args.begin();
                if (it == args.end())
                    return target;

                rttr::type keyType = *it;
                ++it;
                if (it == args.end())
                    return target;

                rttr::type valueType = *it;

                auto srcView = src.create_associative_view();
                for (auto& item : srcView)
                {
                    rttr::variant key = CloneVariant(item.first, keyType, ctx);
                    rttr::variant value = CloneVariant(item.second, valueType, ctx);
                    view.insert(key, value);
                }

                return target;
            }

            if (targetType.is_wrapper())
                return src;

            auto props = targetType.get_properties(
                rttr::filter_item::instance_item |
                rttr::filter_item::public_access |
                rttr::filter_item::non_public_access);

            if (props.begin() == props.end())
                return src;

            rttr::variant target = targetType.create();
            if (!target.is_valid())
                return src;

            CloneObject(src, target, ctx);
            return target;
        }

        ObjPtr<GameObject> CreateCloneShallow(const ObjPtr<GameObject>& original, CloneContext& ctx)
        {
            if (!original.IsValid() || original->IsDestroyed())
                return ObjPtr<GameObject>();

            SceneRef sceneRef = original->GetScene();
            ObjPtr<GameObject> clone = Object::NewObject<GameObject>(sceneRef, original->GetName());

            if (auto sceneRaw = SceneManager::Get().GetSceneRaw(sceneRef))
                sceneRaw->RegisterGameObject(clone);

            clone->SetName(original->GetName());
            clone->SetTag(original->GetTag());
            clone->SetLayer(original->GetLayer());
            clone->SetActive(original->IsActiveSelf());

            ctx.objectMap.emplace(original.operator->(), ObjPtr<Object>(clone));

            auto origTr = original->GetTransform();
            auto cloneTr = clone->GetTransform();
            if (origTr.IsValid() && cloneTr.IsValid())
            {
                ctx.objectMap.emplace(origTr.operator->(), ObjPtr<Object>(cloneTr));
                ctx.transformPairs.emplace_back(origTr, cloneTr);
            }

            // RigidBody를 먼저 만들고, 그 다음 나머지 컴포넌트를 생성한다.
            // Collider가 먼저 만들어지면 자동으로 RigidBody가 생성되어 복제값이 덮이는 문제 방지.
            for (auto& comp : original->GetAllComponents())
            {
                if (!comp.IsValid() || comp->IsDestroyed())
                    continue;

                if (comp.Cast<Transform>())
                    continue;

                rttr::type compType = rttr::type::get(*comp);
                if (compType.get_name().to_string() != "RigidBodyComponent")
                    continue;

                ObjPtr<Component> clonedComp = clone->AddComponent(compType);
                if (!clonedComp.IsValid())
                    continue;

                ctx.objectMap.emplace(comp.operator->(), ObjPtr<Object>(clonedComp));
                ctx.componentPairs.emplace_back(comp, clonedComp);
            }

            for (auto& comp : original->GetAllComponents())
            {
                if (!comp.IsValid() || comp->IsDestroyed())
                    continue;

                if (comp.Cast<Transform>())
                    continue;

                rttr::type compType = rttr::type::get(*comp);
                if (compType.get_name().to_string() == "RigidBodyComponent")
                    continue;

                ObjPtr<Component> clonedComp = clone->AddComponent(compType);
                if (!clonedComp.IsValid())
                    continue;

                ctx.objectMap.emplace(comp.operator->(), ObjPtr<Object>(clonedComp));
                ctx.componentPairs.emplace_back(comp, clonedComp);
            }

            return clone;
        }

        ObjPtr<GameObject> InstantiateGameObjectInternal(const ObjPtr<GameObject>& original, CloneContext& ctx)
        {
            std::vector<ObjPtr<GameObject>> originals;
            CollectHierarchy(original, originals);
            if (originals.empty())
                return ObjPtr<GameObject>();

            ObjPtr<GameObject> rootClone;
            for (auto& go : originals)
            {
                ObjPtr<GameObject> clone = CreateCloneShallow(go, ctx);
                if (!clone.IsValid())
                    continue;

                if (go == original)
                    rootClone = clone;
            }

            for (auto& pair : ctx.transformPairs)
            {
                auto& srcTr = pair.first;
                auto& dstTr = pair.second;
                if (!srcTr.IsValid() || !dstTr.IsValid())
                    continue;

                dstTr->SetLocalPosition(srcTr->GetLocalPosition());
                dstTr->SetLocalRotation(srcTr->GetLocalRotation());
                dstTr->SetLocalScale(srcTr->GetLocalScale());
            }

            for (auto& pair : ctx.componentPairs)
            {
                auto& srcComp = pair.first;
                auto& dstComp = pair.second;
                if (!srcComp.IsValid() || !dstComp.IsValid())
                    continue;

                CloneObject(*srcComp, *dstComp, ctx);

                auto missingSrc = srcComp.Cast<MissingScriptBehaviour>();
                if (missingSrc.IsValid())
                {
                    auto missingDst = dstComp.Cast<MissingScriptBehaviour>();
                    if (missingDst.IsValid())
                        missingDst->SetOriginalPropsMsgPack(missingSrc->GetOriginalPropsMsgPack());
                }
            }

            for (auto& go : originals)
            {
                if (!go.IsValid() || go->IsDestroyed())
                    continue;

                auto origTr = go->GetTransform();
                auto cloneTr = FindMappedTransform(ctx, origTr);
                if (!cloneTr.IsValid())
                    continue;

                const size_t childCount = origTr->GetChildCount();
                for (size_t i = 0; i < childCount; ++i)
                {
                    auto childTr = origTr->GetChild(i);
                    auto childCloneTr = FindMappedTransform(ctx, childTr);
                    if (!childCloneTr.IsValid())
                        continue;

                    childCloneTr->SetParent(cloneTr, false);
                }
            }

            if (rootClone.IsValid())
            {
                auto origRootParent = original->GetTransform()->GetParent();
                if (origRootParent.IsValid() && !origRootParent->IsDestroyed())
                {
                    if (!FindMappedTransform(ctx, origRootParent).IsValid())
                        rootClone->GetTransform()->SetParent(origRootParent, false);
                }
            }

            return rootClone;
        }

        // ---- Prefab instantiate helpers ----
        struct PrefabDeserializeContext
        {
            std::unordered_map<std::string, ObjPtr<Object>> objectTable;
            std::unordered_map<std::string, std::string> muidRemap;
        };

        static bool IsMissingScriptTargetVariant(const rttr::variant& v)
        {
            MMMEngine::Object* o = nullptr;
            if (!v.convert(o) || !o)
                return false;

            rttr::type ot = rttr::type::get(*o);
            return ot.is_derived_from(rttr::type::get<MMMEngine::MissingScriptBehaviour>());
        }

        static std::string RemapMuid(const PrefabDeserializeContext& ctx, const std::string& oldMuid)
        {
            if (oldMuid.empty())
                return {};

            auto it = ctx.muidRemap.find(oldMuid);
            if (it == ctx.muidRemap.end())
                return {};

            return it->second;
        }

        void DeserializeVariantPrefab(rttr::variant& target, const json& j, type target_type,
            const PrefabDeserializeContext& ctx);

        void DeserializeObjectPrefab(rttr::instance obj, const json& j, const PrefabDeserializeContext& ctx)
        {
            type t = obj.get_derived_type();
            bool isObjectDerived = (t.is_derived_from(type::get<Object>()) || t == type::get<Object>());

            for (auto& prop : t.get_properties(
                rttr::filter_item::instance_item |
                rttr::filter_item::public_access |
                rttr::filter_item::non_public_access))
            {
                if (prop.is_readonly())
                    continue;

                std::string propName = prop.get_name().to_string();
                if (isObjectDerived && (propName == "MUID" || propName == "m_muid"))
                    continue;

                if (!j.contains(propName))
                    continue;

                rttr::variant currentValue = prop.get_value(obj);
                DeserializeVariantPrefab(currentValue, j[propName], prop.get_type(), ctx);
                prop.set_value(obj, currentValue);
            }
        }

        void DeserializeVariantPrefab(rttr::variant& target, const json& j, type target_type,
            const PrefabDeserializeContext& ctx)
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
                    if (innerType.is_derived_from(rttr::type::get<Resource>()) ||
                        innerType == rttr::type::get<Resource>())
                    {
                        std::string pathStr = j.get<std::string>();
                        std::wstring filePath = Utility::StringHelper::StringToWString(pathStr);

                        rttr::variant loadedResource = ResourceManager::Get().Load(innerType, filePath);
                        if (loadedResource.convert(target.get_type()))
                            target = loadedResource;
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

                    if (!isObjPtrWrapper)
                    {
                        rttr::type raw_wrapped = wrapped.get_raw_type();
                        if (!raw_wrapped.is_valid())
                            raw_wrapped = wrapped;
                        rttr::variant unwrapped = target.extract_wrapped_value();
                        if (!unwrapped.is_valid() || unwrapped.get_type() != raw_wrapped)
                            unwrapped = raw_wrapped.create();
                        DeserializeVariantPrefab(unwrapped, j, raw_wrapped, ctx);
                        target = unwrapped;
                        return;
                    }
                }
            }

            if (target_type.is_enumeration())
            {
                if (j.contains("EnumType") && j.contains("EnumValue"))
                {
                    std::string enumValueName = j["EnumValue"].get<std::string>();
                    rttr::enumeration enumType = target_type.get_enumeration();
                    rttr::variant enumValue = enumType.name_to_value(enumValueName);
                    if (enumValue.is_valid())
                        target = enumValue;
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



            if (target_type == type::get<MMMEngine::SerializableEvent>())
            {
                std::vector<MMMEngine::PersistentCall> calls;
                if (j.is_array())
                {
                    for (const auto& item : j)
                    {
                        std::string oldMuid = item.contains("TargetMUID") ? item["TargetMUID"].get<std::string>() : "";
                        std::string messageName = item.contains("MessageName") ? item["MessageName"].get<std::string>() : "";
                        std::string newMuid = RemapMuid(ctx, oldMuid);
                        calls.emplace_back(std::move(newMuid), std::move(messageName));
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
                        std::string oldMuid = item.contains("TargetMUID") ? item["TargetMUID"].get<std::string>() : "";
                        std::string messageName = item.contains("MessageName") ? item["MessageName"].get<std::string>() : "";
                        std::string newMuid = RemapMuid(ctx, oldMuid);
                        calls.emplace_back(std::move(newMuid), std::move(messageName));
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
                        std::string oldMuid = item.contains("TargetMUID") ? item["TargetMUID"].get<std::string>() : "";
                        std::string messageName = item.contains("MessageName") ? item["MessageName"].get<std::string>() : "";
                        std::string newMuid = RemapMuid(ctx, oldMuid);
                        calls.emplace_back(std::move(newMuid), std::move(messageName));
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
                        std::string oldMuid = item.contains("TargetMUID") ? item["TargetMUID"].get<std::string>() : "";
                        std::string messageName = item.contains("MessageName") ? item["MessageName"].get<std::string>() : "";
                        std::string newMuid = RemapMuid(ctx, oldMuid);
                        calls.emplace_back(std::move(newMuid), std::move(messageName));
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
                        std::string oldMuid = item.contains("TargetMUID") ? item["TargetMUID"].get<std::string>() : "";
                        std::string messageName = item.contains("MessageName") ? item["MessageName"].get<std::string>() : "";
                        std::string newMuid = RemapMuid(ctx, oldMuid);
                        calls.emplace_back(std::move(newMuid), std::move(messageName));
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
                    DeserializeVariantPrefab(element, item, value_type, ctx);
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
                    DeserializeObjectPrefab(k, keyJson, ctx);
                    DeserializeObjectPrefab(v, value, ctx);

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
                    DeserializeVariantPrefab(keyframes, j, keyframesProp.get_type(), ctx);
                    keyframesProp.set_value(target, keyframes);
                    return;
                }
            }

            rttr::type inject_type = ResolveObjPtrInjectType(target_type);
            if (inject_type.is_valid())
            {
                auto inject = inject_type.get_method("Inject");
                if (!inject.is_valid())
                {
                    target = rttr::variant();
                    return;
                }

                if (j.is_null())
                {
                    ObjPtr<Object> nullObj;
                    const ObjPtrBase& nullRef = nullObj;
                    inject.invoke(target, nullRef);
                    return;
                }

                if (!j.is_string())
                {
                    ObjPtr<Object> nullObj;
                    const ObjPtrBase& nullRef = nullObj;
                    inject.invoke(target, nullRef);
                    return;
                }
                std::string muidStr = j.get<std::string>();
                auto tryInject = [&](const ObjPtr<Object>& obj) -> bool
                {
                    if (!obj.IsValid())
                        return false;
                    if (obj.Cast<MissingScriptBehaviour>().IsValid())
                        return false;

                    const ObjPtrBase& baseRef = obj;
                    inject.invoke(target, baseRef);
                    return true;
                };

                // 1) Prefab 내부 오브젝트 우선
                auto it = ctx.objectTable.find(muidStr);
                if (it != ctx.objectTable.end() && !IsMissingScriptTargetVariant(it->second))
                {
                    rttr::variant src = it->second;
                    if (src.is_type<ObjPtr<Object>>())
                    {
                        ObjPtr<Object> base = src.get_value<ObjPtr<Object>>();
                        if (tryInject(base))
                            return;
                    }
                }

                // 2) 내부 MUID가 리맵된 경우 (예외적 누락 대비)
                std::string remapped = RemapMuid(ctx, muidStr);
                if (!remapped.empty())
                {
                    ObjPtr<Object> remappedObj = ObjectManager::Get().GetObjectByMUID(remapped);
                    if (tryInject(remappedObj))
                        return;
                }

                // 3) 프리팹 외부 오브젝트 참조는 전역 MUID로 복원 시도
                ObjPtr<Object> externalObj = ObjectManager::Get().GetObjectByMUID(muidStr);
                if (tryInject(externalObj))
                    return;

                ObjPtr<Object> nullObj;
                const ObjPtrBase& nullRef = nullObj;
                inject.invoke(target, nullRef);
                return;
            }

            if (!target.is_valid() || target.get_type() != target_type)
            {
                target = target_type.create();
            }

            DeserializeObjectPrefab(target, j, ctx);
        }

        struct PendingComponentProps
        {
            ObjPtr<Component> comp;
            const json* props = nullptr;
        };

        ObjPtr<Component> CreateComponentForDeserializePrefab(const json& compJson, ObjPtr<GameObject> obj,
            bool& outIsMissing, PrefabDeserializeContext& ctx)
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
                    ctx.objectTable[muid] = ObjPtr<Object>(compVar);
                    ctx.muidRemap[muid] = compVar->GetMUID().ToString();
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
                ctx.objectTable[muid] = ObjPtr<Object>(comp);
                ctx.muidRemap[muid] = comp->GetMUID().ToString();
            }

            return comp;
        }

        void DeserializeTransformPrefab(Transform& tr, const json& j, const type& t,
            const PrefabDeserializeContext& ctx)
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
                DeserializeVariantPrefab(v, j[name], prop.get_type(), ctx);
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

        // ---- Prefab serialize helpers ----
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
                if (t == type::get<uint64_t>()) return var.to_uint64();
                if (t == type::get<float>()) return var.to_float();
                if (t == type::get<double>()) return var.to_double();
            }



            if (t == type::get<MMMEngine::Utility::MUID>())
            {
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
                    obj[SerializeVariant(item.first).dump()] = SerializeVariant(item.second);
                }
                return obj;
            }

            json out;
            for (auto& prop : t.get_properties(
                rttr::filter_item::instance_item |
                rttr::filter_item::public_access |
                rttr::filter_item::non_public_access))
            {
                if (prop.is_readonly())
                    continue;

                rttr::variant value = prop.get_value(var);
                out[prop.get_name().to_string()] = SerializeVariant(value);
            }

            return out;
        }

        json SerializeComponent(const ObjPtr<Component>& comp)
        {
            json compJson;
            type type = type::get(*comp);
            compJson["Type"] = type.get_name().to_string();

            ObjPtr<MissingScriptBehaviour> missing;
            try { missing = comp.Cast<MissingScriptBehaviour>(); }
            catch (...) { /* ignore */ }

            if (missing.IsValid())
            {
                const std::string& originalType = missing->GetOriginalTypeName();
                compJson["Type"] = originalType.empty() ? std::string("MissingScriptBehaviour") : originalType;

                if (missing->HasOriginalProps())
                {
                    json props = json::from_msgpack(missing->GetOriginalPropsMsgPack());
                    compJson["Props"] = props;
                }
                else
                {
                    compJson["Props"] = json::object();
                    compJson["Props"]["MUID"] = comp->GetMUID().ToString();
                }

                return compJson;
            }

            for (auto& prop : type.get_properties(
                rttr::filter_item::instance_item |
                rttr::filter_item::public_access |
                rttr::filter_item::non_public_access))
            {
                if (prop.is_readonly())
                    continue;

                rttr::variant value = prop.get_value(*comp);
                compJson["Props"][prop.get_name().to_string()] = SerializeVariant(value);
            }

            return compJson;
        }

        std::filesystem::path MakeFileUnique(const std::filesystem::path& parentDir,
            const std::string& fileName, const std::string& extension)
        {
            std::filesystem::path path = parentDir / (fileName + extension);
            for (int i = 1; i < 100; i++)
            {
                if (!std::filesystem::exists(path))
                    break;
                path = parentDir / (fileName + "_" + std::to_string(i) + extension);
            }
            return path;
        }
    }

    ObjPtr<GameObject> ObjectSerializer::Instantiate(const ObjPtr<GameObject>& original)
    {
        if (!original.IsValid() || original->IsDestroyed())
            return ObjPtr<GameObject>();

        CloneContext ctx;
        return InstantiateGameObjectInternal(original, ctx);
    }

    ObjPtr<Component> ObjectSerializer::Instantiate(const ObjPtr<Component>& original)
    {
        if (!original.IsValid() || original->IsDestroyed())
            return ObjPtr<Component>();

        auto owner = original->GetGameObject();
        if (!owner.IsValid() || owner->IsDestroyed())
            return ObjPtr<Component>();

        CloneContext ctx;
        ObjPtr<GameObject> clonedRoot = InstantiateGameObjectInternal(owner, ctx);
        if (!clonedRoot.IsValid())
            return ObjPtr<Component>();

        auto it = ctx.objectMap.find(original.operator->());
        if (it != ctx.objectMap.end())
            return it->second.Cast<Component>();

        rttr::type originalType = rttr::type::get(*original);
        for (auto& comp : clonedRoot->GetAllComponents())
        {
            if (!comp.IsValid() || comp->IsDestroyed())
                continue;

            if (rttr::type::get(*comp) == originalType)
                return comp;
        }

        return ObjPtr<Component>();
    }

    ObjPtr<GameObject> ObjectSerializer::Instantiate(const ResPtr<Prefab>& prefab)
    {
        if (!prefab)
            return ObjPtr<GameObject>();

        const SnapShot& snapshot = prefab->GetSnapshot();
        if (!snapshot.contains("GameObjects"))
            return ObjPtr<GameObject>();

        const auto& gameObjects = snapshot["GameObjects"];
        if (!gameObjects.is_array() || gameObjects.empty())
            return ObjPtr<GameObject>();

        SceneRef sceneRef = SceneManager::Get().GetCurrentScene();
        Scene* sceneRaw = SceneManager::Get().GetSceneRaw(sceneRef);
        if (!sceneRaw)
            return ObjPtr<GameObject>();

        PrefabDeserializeContext ctx;
        std::unordered_map<std::string, std::string> pendingParent;
        std::vector<ObjPtr<GameObject>> createdGameObjects;

        std::string rootMuid = gameObjects[0].contains("MUID")
            ? gameObjects[0]["MUID"].get<std::string>()
            : std::string();

        for (const auto& goJson : gameObjects)
        {
            std::string goName = goJson.contains("Name") ? goJson["Name"].get<std::string>() : "GameObject";
            std::string goMUID = goJson.contains("MUID") ? goJson["MUID"].get<std::string>() : "";
            uint32_t goLayer = goJson.contains("Layer") ? goJson["Layer"].get<uint32_t>() : 0;
            std::string goTag = goJson.contains("Tag") ? goJson["Tag"].get<std::string>() : "";
            bool active = goJson.contains("Active") ? goJson["Active"].get<bool>() : true;

            ObjPtr<GameObject> go = Object::NewObject<GameObject>(sceneRef, goName);
            if (auto currentScene = SceneManager::Get().GetSceneRaw(sceneRef))
                currentScene->RegisterGameObject(go);
            go->SetName(goName);
            go->SetLayer(goLayer);
            go->SetTag(goTag);
            go->SetActive(active);
            createdGameObjects.push_back(go);

            if (!goMUID.empty())
            {
                ctx.objectTable[goMUID] = ObjPtr<Object>(go);
                ctx.muidRemap[goMUID] = go->GetMUID().ToString();
            }

            if (!goJson.contains("Components"))
                continue;

            const nlohmann::json& components = goJson["Components"];
            TransformCompInfo trCompInfo = FindTransformComp(components);
            if (!trCompInfo.comp || !trCompInfo.comp->contains("Props"))
                continue;

            const nlohmann::json& trProps = (*trCompInfo.comp)["Props"];

            if (trCompInfo.isRect)
                go->EnsureRectTransform();

            auto tr = go->GetTransform();
            if (!tr.IsValid())
                continue;

            if (trProps.contains("MUID"))
            {
                std::string trMUID = trProps["MUID"].get<std::string>();
                if (!trMUID.empty())
                {
                    ctx.objectTable[trMUID] = ObjPtr<Object>(tr);
                    ctx.muidRemap[trMUID] = tr->GetMUID().ToString();
                }
            }

            auto trType = trCompInfo.isRect ? type::get<RectTransform>() : type::get<Transform>();
            DeserializeTransformPrefab(*tr, trProps, trType, ctx);

            if (trProps.contains("Parent") && trProps.contains("MUID") && !trProps["Parent"].is_null())
                pendingParent[trProps["MUID"].get<std::string>()] = trProps["Parent"].get<std::string>();
        }

        std::vector<PendingComponentProps> pendingComponentProps;

        for (const auto& goJson : gameObjects)
        {
            if (!goJson.contains("MUID"))
                continue;

            std::string goMUID = goJson["MUID"].get<std::string>();
            auto itGo = ctx.objectTable.find(goMUID);
            if (itGo == ctx.objectTable.end())
                continue;

            ObjPtr<GameObject> go = itGo->second.Cast<GameObject>();
            if (!goJson.contains("Components"))
                continue;

            const nlohmann::json& components = goJson["Components"];

            for (const auto& compJson : components)
            {
                if (!compJson.contains("Type"))
                    continue;

                std::string typeName = compJson["Type"].get<std::string>();
                if (typeName != "RigidBodyComponent")
                    continue;

                bool isMissing = false;
                ObjPtr<Component> comp = CreateComponentForDeserializePrefab(compJson, go, isMissing, ctx);
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

            for (const auto& compJson : components)
            {
                if (!compJson.contains("Type"))
                    continue;

                std::string typeName = compJson["Type"].get<std::string>();
                if (typeName == "Transform" || typeName == "RectTransform" || typeName == "RigidBodyComponent")
                    continue;

                bool isMissing = false;
                ObjPtr<Component> comp = CreateComponentForDeserializePrefab(compJson, go, isMissing, ctx);
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

        for (auto& pending : pendingComponentProps)
        {
            if (!pending.comp.IsValid() || pending.comp->IsDestroyed())
                continue;

            if (!pending.props)
                continue;

            DeserializeObjectPrefab(*pending.comp, *pending.props, ctx);
        }

        for (auto& [childTrMUID, parentTrMUID] : pendingParent)
        {
            auto itChild = ctx.objectTable.find(childTrMUID);
            auto itParent = ctx.objectTable.find(parentTrMUID);
            if (itChild == ctx.objectTable.end() || itParent == ctx.objectTable.end())
                continue;

            auto childTr = itChild->second.Cast<Transform>();
            auto parentTr = itParent->second.Cast<Transform>();
            childTr->SetParent(parentTr, false);
        }

        // UI 그래픽은 부모 체인을 기준으로 Canvas를 다시 찾는다.
        for (auto& go : createdGameObjects)
        {
            if (!go.IsValid())
                continue;

            for (auto& comp : go->GetAllComponents())
            {
                if (!comp.IsValid() || comp->IsDestroyed())
                    continue;

                if (auto graphic = comp.Cast<Graphic>(); graphic.IsValid())
                    graphic->RefreshCanvasNow();
            }
        }

        SerializableEvent::SetResolver([](const Utility::MUID& muid) { return ObjectManager::Get().GetObjectByMUID(muid); });
        SerializableEventT<float>::SetResolver([](const Utility::MUID& muid) { return ObjectManager::Get().GetObjectByMUID(muid); });
        SerializableEventT<bool>::SetResolver([](const Utility::MUID& muid) { return ObjectManager::Get().GetObjectByMUID(muid); });
        SerializableEventT<int>::SetResolver([](const Utility::MUID& muid) { return ObjectManager::Get().GetObjectByMUID(muid); });
        SerializableEventT<std::string>::SetResolver([](const Utility::MUID& muid) { return ObjectManager::Get().GetObjectByMUID(muid); });

        auto tryInitializeBehaviours = []()
        {
            if (!GlobalRegistry::g_runtimeActive)
                return;

            BehaviourManager::Get().InitializeBehaviours();
        };

        ObjPtr<GameObject> root;
        if (!rootMuid.empty())
        {
            auto itRoot = ctx.objectTable.find(rootMuid);
            if (itRoot != ctx.objectTable.end())
                root = itRoot->second.Cast<GameObject>();
        }

        tryInitializeBehaviours();
        return root;
    }

    bool ObjectSerializer::CreatePrefabFromGameObject(const ObjPtr<GameObject>& root,
        const std::filesystem::path& directory)
    {
        if (!root.IsValid() || root->IsDestroyed())
            return false;

        std::vector<ObjPtr<GameObject>> gameObjects;
        CollectHierarchy(root, gameObjects);
        if (gameObjects.empty())
            return false;

        json snapshot;
        json goArray = json::array();

        for (auto& goPtr : gameObjects)
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
            for (auto& comp : goPtr->GetAllComponents())
            {
                compArray.push_back(SerializeComponent(comp));
            }
            goJson["Components"] = compArray;

            goArray.push_back(goJson);
        }

        snapshot["GameObjects"] = goArray;
        std::vector<uint8_t> v = json::to_msgpack(snapshot);

        std::filesystem::path dirPath = directory;
        if (!dirPath.empty() && !std::filesystem::exists(dirPath))
            std::filesystem::create_directories(dirPath);

        std::filesystem::path outPath = MakeFileUnique(dirPath, root->GetName(), ".Prefab");
        std::ofstream file(outPath, std::ios::binary);
        if (!file.is_open())
            return false;

        file.write(reinterpret_cast<const char*>(v.data()), v.size());
        file.close();

        return true;
    }
}
