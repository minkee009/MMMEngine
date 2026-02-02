#include "InspectorWindow.h"
#include "SceneManager.h"
#include "ObjectManager.h"
#include "Transform.h"
#include "RectTransform.h"
#include "Resource.h"
#include "RigidBodyComponent.h"
#include "Behaviour.h"
#include "SerializableEvent.h"
#include "Canvas.h"
#include <regex>

#include "EditorRegistry.h"
using namespace MMMEngine::EditorRegistry;
using namespace DirectX::SimpleMath;
using namespace DirectX;
#include "DragAndDrop.h"
#include "StringHelper.h"
#include "ProjectManager.h"
#include "ResourceManager.h"
#include <rttr/variant_sequential_view.h>
#include <algorithm>
#include <optional>
#include <iterator>
#include <unordered_set>
#include <cctype>

using namespace MMMEngine;
using namespace MMMEngine::Editor;
using namespace MMMEngine::Utility;

static void ApplyRigidBodyFromTransformIfPlaying(const ObjPtr<GameObject>& go)
{
    if (!g_editor_scene_playing)
        return;

    if (!go.IsValid())
        return;

    auto rbPtr = go->GetComponent<RigidBodyComponent>();
    if (!rbPtr.IsValid())
        return;

    auto tr = go->GetTransform();
    if (!tr.IsValid())
        return;

    if (rbPtr->GetKinematic())
        rbPtr->SetKinematicTarget(tr->GetWorldPosition(), tr->GetWorldRotation());
    else
        rbPtr->Editor_changeTrans(tr->GetWorldPosition(), tr->GetWorldRotation());
}

namespace
{
    static std::string TrimCopy(const std::string& s)
    {
        size_t start = 0;
        while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start])))
            ++start;
        size_t end = s.size();
        while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1])))
            --end;
        return s.substr(start, end - start);
    }

    static std::string ToLowerCopy(std::string s)
    {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return s;
    }

    static std::vector<std::string> Split(const std::string& s, char delim)
    {
        std::vector<std::string> out;
        std::string current;
        for (char c : s)
        {
            if (c == delim)
            {
                out.push_back(current);
                current.clear();
            }
            else
            {
                current.push_back(c);
            }
        }
        out.push_back(current);
        return out;
    }

    static std::string NormalizeBoolKey(const std::string& key)
    {
        std::string k = ToLowerCopy(TrimCopy(key));
        if (k == "true" || k == "1" || k == "yes" || k == "on")
            return "true";
        if (k == "false" || k == "0" || k == "no" || k == "off")
            return "false";
        if (k == "*")
            return "*";
        return k;
    }

    struct InspectorChainMapping
    {
        std::unordered_map<std::string, std::vector<std::string>> valueToTargets;
        std::unordered_set<std::string> allTargets;
    };

    static InspectorChainMapping ParseInspectorChainMapping(const std::string& raw, const std::string& defaultKey, bool normalizeBoolKeys)
    {
        InspectorChainMapping mapping;
        for (const auto& entryRaw : Split(raw, ';'))
        {
            std::string entry = TrimCopy(entryRaw);
            if (entry.empty())
                continue;

            std::string key;
            std::string rhs;
            size_t eqPos = entry.find('=');
            if (eqPos == std::string::npos)
            {
                key = defaultKey;
                rhs = entry;
            }
            else
            {
                key = TrimCopy(entry.substr(0, eqPos));
                rhs = TrimCopy(entry.substr(eqPos + 1));
            }

            if (normalizeBoolKeys)
                key = NormalizeBoolKey(key);

            if (key.empty() || rhs.empty())
                continue;

            auto& targets = mapping.valueToTargets[key];
            for (const auto& propRaw : Split(rhs, ','))
            {
                std::string propName = TrimCopy(propRaw);
                if (propName.empty())
                    continue;
                targets.push_back(propName);
                mapping.allTargets.insert(propName);
            }
        }
        return mapping;
    }

    static std::unordered_set<std::string> BuildAllowedTargets(const InspectorChainMapping& mapping, const std::vector<std::string>& keys)
    {
        std::unordered_set<std::string> allowed;
        auto addTargets = [&](const std::string& key)
        {
            auto it = mapping.valueToTargets.find(key);
            if (it == mapping.valueToTargets.end())
                return;
            for (const auto& name : it->second)
                allowed.insert(name);
        };

        addTargets("*");
        for (const auto& key : keys)
            addTargets(key);
        return allowed;
    }

    static bool TryGetVariantInt64(const rttr::variant& var, int64_t& out)
    {
        if (!var.is_valid())
            return false;
        if (var.can_convert<int>())
        {
            out = static_cast<int64_t>(var.to_int());
            return true;
        }
        if (var.can_convert<int64_t>())
        {
            out = var.get_value<int64_t>();
            return true;
        }
        return false;
    }

    static bool IsNumericString(const std::string& s)
    {
        if (s.empty())
            return false;
        size_t i = 0;
        if (s[0] == '-' || s[0] == '+')
            i = 1;
        if (i >= s.size())
            return false;
        for (; i < s.size(); ++i)
        {
            if (!std::isdigit(static_cast<unsigned char>(s[i])))
                return false;
        }
        return true;
    }

    static std::unordered_map<std::string, bool> BuildInspectorChainVisibility(rttr::instance inst, const rttr::type& t)
    {
        std::unordered_map<std::string, bool> visibility;

        for (auto& prop : t.get_properties())
        {
            rttr::variant md = prop.get_metadata("INSPECTOR_CHAIN");
            if (!md.is_valid() || !md.is_type<std::string>())
                continue;

            const std::string raw = md.get_value<std::string>();
            if (raw.empty())
                continue;

            rttr::variant controllerVar = prop.get_value(inst);
            if (!controllerVar.is_valid())
                continue;

            const std::string controllerName = prop.get_name().to_string();
            rttr::type controllerType = prop.get_type();
            std::unordered_set<std::string> allowed;
            InspectorChainMapping mapping;

            if (controllerType == rttr::type::get<bool>())
            {
                mapping = ParseInspectorChainMapping(raw, "true", true);
                const std::string key = controllerVar.to_bool() ? "true" : "false";
                allowed = BuildAllowedTargets(mapping, { key });
            }
            else if (controllerType.is_enumeration())
            {
                mapping = ParseInspectorChainMapping(raw, "*", false);
                rttr::enumeration enumType = controllerType.get_enumeration();
                std::string currentName = enumType.value_to_name(controllerVar).to_string();
                std::string currentLower = ToLowerCopy(currentName);

                std::vector<std::string> matchedKeys;
                for (const auto& entry : mapping.valueToTargets)
                {
                    const std::string& key = entry.first;
                    if (key == "*")
                        continue;
                    if (key == currentName || ToLowerCopy(key) == currentLower)
                    {
                        matchedKeys.push_back(key);
                        continue;
                    }
                    if (IsNumericString(key))
                    {
                        int64_t currentValue = 0;
                        if (TryGetVariantInt64(controllerVar, currentValue))
                        {
                            if (std::stoll(key) == currentValue)
                                matchedKeys.push_back(key);
                        }
                    }
                }
                allowed = BuildAllowedTargets(mapping, matchedKeys);
            }
            else
            {
                continue;
            }

            for (const auto& target : mapping.allTargets)
            {
                if (target == controllerName)
                    continue;

                bool allow = allowed.find(target) != allowed.end();
                auto it = visibility.find(target);
                if (it == visibility.end())
                    visibility[target] = allow;
                else
                    it->second = it->second && allow;
            }
        }

        return visibility;
    }
}

/// 단순 타입(Vector2/3/4, float, int, bool, Color, enum) 그리기 및 편집. var를 갱신하고, 처리한 타입이면 true.
/// prop/inst가 주어지면 float에 MIN/MAX 메타데이터 적용. (sequential 요소용으로는 nullptr/빈 instance 전달)
static bool DrawSimplePropertyValue(const char* label, rttr::variant& var, rttr::type propType, bool readOnly,
    bool* outChanged, const rttr::property* prop = nullptr, rttr::instance inst = rttr::instance())
{
    bool changed = false;
    if (outChanged) *outChanged = false;

    if (propType == rttr::type::get<int>())
    {
        int v = var.to_int();
        if (readOnly) ImGui::BeginDisabled(true);
        changed = ImGui::DragInt(label, &v);
        if (readOnly) ImGui::EndDisabled();
        if (changed && !readOnly) var = v;
    }
    else if (propType == rttr::type::get<float>())
    {
        float v = static_cast<float>(var.to_double());
        float minVal = 0.0f, maxVal = 0.0f;
        if (prop && inst.is_valid())
        {
            rttr::variant mdMin = prop->get_metadata("MIN"), mdMax = prop->get_metadata("MAX");
            if (mdMin.is_valid() && mdMin.can_convert<float>()) minVal = mdMin.get_value<float>();
            if (mdMax.is_valid() && mdMax.can_convert<float>()) maxVal = mdMax.get_value<float>();
        }
        if (readOnly) ImGui::BeginDisabled(true);
        changed = ImGui::DragFloat(label, &v, 0.01f, minVal, maxVal);
        if (readOnly) ImGui::EndDisabled();
        if (changed && !readOnly) var = v;
    }
    else if (propType == rttr::type::get<bool>())
    {
        bool v = var.to_bool();
        if (readOnly) ImGui::BeginDisabled(true);
        changed = ImGui::Checkbox(label, &v);
        if (readOnly) ImGui::EndDisabled();
        if (changed && !readOnly) var = v;
    }
    else if (propType == rttr::type::get<Vector2>())
    {
        auto v = var.get_value<Vector2>();
        float d[2] = { v.x, v.y };
        if (readOnly) ImGui::BeginDisabled(true);
        changed = ImGui::DragFloat2(label, d, 0.1f);
        if (readOnly) ImGui::EndDisabled();
        if (changed && !readOnly) var = Vector2(d[0], d[1]);
    }
    else if (propType == rttr::type::get<Vector3>())
    {
        auto v = var.get_value<Vector3>();
        float d[3] = { v.x, v.y, v.z };
        if (readOnly) ImGui::BeginDisabled(true);
        changed = ImGui::DragFloat3(label, d, 0.1f);
        if (readOnly) ImGui::EndDisabled();
        if (changed && !readOnly) var = Vector3(d[0], d[1], d[2]);
    }
    else if (propType == rttr::type::get<Vector4>())
    {
        auto v = var.get_value<Vector4>();
        float d[4] = { v.x, v.y, v.z, v.w };
        if (readOnly) ImGui::BeginDisabled(true);
        changed = ImGui::DragFloat4(label, d, 0.1f);
        if (readOnly) ImGui::EndDisabled();
        if (changed && !readOnly) var = Vector4(d[0], d[1], d[2], d[3]);
    }
    else if (propType == rttr::type::get<Color>())
    {
        Color c = var.get_value<Color>();
        float rgba[4] = { c.x, c.y, c.z, c.w };
        if (readOnly) ImGui::BeginDisabled(true);
        changed = ImGui::ColorEdit4(label, rgba, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_AlphaBar);
        if (readOnly) ImGui::EndDisabled();
        if (changed && !readOnly) var = Color(rgba[0], rgba[1], rgba[2], rgba[3]);
    }
    else if (propType.is_enumeration())
    {
        rttr::enumeration enumType = propType.get_enumeration();
        std::string currentName = enumType.value_to_name(var).to_string();
        if (readOnly) ImGui::BeginDisabled(true);
        if (ImGui::BeginCombo(label, currentName.c_str()))
        {
            for (const auto& enumName : enumType.get_names())
            {
                std::string enumNameStr = enumName.to_string();
                bool isSelected = (enumNameStr == currentName);
                if (ImGui::Selectable(enumNameStr.c_str(), isSelected))
                {
                    if (!readOnly)
                    {
                        rttr::variant newVal = enumType.name_to_value(enumName);
                        if (newVal.is_valid()) { var = newVal; changed = true; }
                    }
                }
                if (isSelected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        if (readOnly) ImGui::EndDisabled();
    }
    else
    {
        return false; // 미지원 타입
    }

    if (outChanged) *outChanged = changed;
    return true;
}

static ObjPtr<Object> ResolveObjectByMUID(const std::string& muidStr)
{
    if (muidStr.empty())
        return nullptr;

    if (auto obj = ObjectManager::Get().GetObjectByMUID(muidStr); obj.IsValid())
        return obj;

    auto parsed = Utility::MUID::Parse(muidStr);
    if (!parsed.has_value() || parsed->IsEmpty())
        return nullptr;

    const Utility::MUID& targetMuid = parsed.value();

    auto scanGameObjects = [&](const std::vector<ObjPtr<GameObject>>& gameObjects) -> ObjPtr<Object>
    {
        for (const auto& go : gameObjects)
        {
            if (!go.IsValid())
                continue;

            if (go->GetMUID() == targetMuid)
                return go;

            ObjPtr<Transform> tr = go->GetTransform();
            if (tr.IsValid() && tr->GetMUID() == targetMuid)
                return tr;

            for (const auto& comp : go->GetAllComponents())
            {
                if (comp.IsValid() && comp->GetMUID() == targetMuid)
                    return comp;
            }
        }
        return nullptr;
    };

    if (auto obj = scanGameObjects(SceneManager::Get().GetAllGameObjectInCurrentScene()); obj.IsValid())
        return obj;

    return scanGameObjects(SceneManager::Get().GetAllGameObjectInDDOL());
}

static ObjPtr<GameObject> GetTargetGameObjectFromMUID(const std::string& muidStr)
{
    if (muidStr.empty())
        return nullptr;

    ObjPtr<Object> obj = ResolveObjectByMUID(muidStr);
    if (!obj.IsValid())
        return nullptr;

    if (auto goPtr = obj.Cast<GameObject>(); goPtr.IsValid())
        return goPtr;

    if (auto compPtr = obj.Cast<Component>(); compPtr.IsValid())
        return compPtr->GetGameObject();

    return nullptr;
}

static std::string GetDisplayNameForMUID(const std::string& muidStr)
{
    if (muidStr.empty())
        return "Drop GameObject";

    ObjPtr<Object> obj = ResolveObjectByMUID(muidStr);
    if (!obj.IsValid())
        return "Missing";

    if (auto go = obj.Cast<GameObject>(); go.IsValid())
        return go->GetName();

    if (auto comp = obj.Cast<Component>(); comp.IsValid())
    {
        std::string typeName = rttr::type::get(*comp).get_name().to_string();
        ObjPtr<GameObject> owner = comp->GetGameObject();
        if (owner.IsValid())
            return owner->GetName() + " (" + typeName + ")";

        return typeName;
    }

    return "Missing";
}

static std::string GetMessagePreviewLabel(const PersistentCall& call)
{
    if (call.GetMessageName().empty())
        return "None";

    ObjPtr<Object> obj = ResolveObjectByMUID(call.GetTargetMUID());
    if (!obj.IsValid())
        return call.GetMessageName();

    ObjPtr<Behaviour> behaviour = obj.Cast<Behaviour>();
    if (!behaviour.IsValid())
        return call.GetMessageName();

    std::string typeName = rttr::type::get(*behaviour).get_name().to_string();
    return typeName + "::" + call.GetMessageName();
}

struct EventMessageOption
{
    std::string label;
    std::string messageName;
    MMMEngine::Utility::MUID targetMUID;
};

static bool IsScreenSpaceCanvasRoot(const ObjPtr<GameObject>& go, Canvas*& outCanvas)
{
    outCanvas = nullptr;
    if (!go.IsValid())
        return false;

    auto canvas = go->GetComponent<Canvas>();
    if (!canvas.IsValid())
        return false;

    outCanvas = canvas.operator->(); //WTF??
    return true;
}

static void CollectEventMessageOptions(const ObjPtr<GameObject>& go, bool floatEvent,
    std::vector<EventMessageOption>& outOptions)
{
    outOptions.clear();
    if (!go.IsValid())
        return;

    for (auto& comp : go->GetAllComponents())
    {
        ObjPtr<Behaviour> behaviour = comp.Cast<Behaviour>();
        if (!behaviour.IsValid())
            continue;

        std::vector<std::string> messageNames;
        if (floatEvent)
            behaviour->GetFloatMessageNames(messageNames);
        else
            behaviour->GetVoidMessageNames(messageNames);

        if (messageNames.empty())
            continue;

        std::string typeName = rttr::type::get(*behaviour).get_name().to_string();
        for (const auto& msgName : messageNames)
        {
            EventMessageOption option;
            option.label = typeName + "::" + msgName;
            option.messageName = msgName;
            option.targetMUID = behaviour->GetMUID();
            outOptions.push_back(std::move(option));
        }
    }

    std::sort(outOptions.begin(), outOptions.end(),
        [](const EventMessageOption& a, const EventMessageOption& b)
        {
            return a.label < b.label;
        });
}

/// SerializableEvent / SerializableEventT<float> 전용 그리기. 처리한 타입이면 true.
static bool DrawSerializableEventProperty(const std::string& name, rttr::variant& var, rttr::type propType,
    const rttr::property& prop, rttr::instance inst, bool readOnly)
{
    auto drawCalls = [&](std::vector<PersistentCall>& calls, bool floatEvent)
    {
        ImGui::Indent(ImGui::GetTreeNodeToLabelSpacing());
        std::string headerLabel = name + "  [" + std::to_string(calls.size()) + "]###" + name;
        bool opened = ImGui::CollapsingHeader(headerLabel.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
        if (opened)
        {
            ImGui::Indent(ImGui::GetTreeNodeToLabelSpacing());
            for (size_t i = 0; i < calls.size(); ++i)
            {
                ImGui::PushID(static_cast<int>(i));

                // Target selection (drag & drop only)
                std::string targetLabel = GetDisplayNameForMUID(calls[i].GetTargetMUID());
                ImGui::Text("Target");
                ImGui::SameLine();
                ImGui::PushID("TargetBtn");
                if (readOnly) ImGui::BeginDisabled(true);
                ImGui::Button(targetLabel.c_str(), ImVec2(-1, 0));
                if (readOnly) ImGui::EndDisabled();

                if (!readOnly)
                {
                    Utility::MUID dropped = GetMuid("gameobject_muid");
                    if (dropped.IsValid())
                    {
                        calls[i].SetTargetMUID(dropped.ToString());
                        calls[i].SetMessageName("");
                    }
                }

                if (ImGui::BeginPopupContextItem("EventTargetContext"))
                {
                    if (!readOnly && ImGui::MenuItem(u8"참조 해제"))
                    {
                        calls[i].SetTargetMUID("");
                        calls[i].SetMessageName("");
                    }
                    ImGui::EndPopup();
                }
                ImGui::PopID();

                ObjPtr<GameObject> targetGo = GetTargetGameObjectFromMUID(calls[i].GetTargetMUID());
                std::vector<EventMessageOption> options;
                CollectEventMessageOptions(targetGo, floatEvent, options);

                std::string previewLabel = GetMessagePreviewLabel(calls[i]);
                if (!targetGo.IsValid())
                    previewLabel = "Drop GameObject";
                else if (calls[i].GetMessageName().empty())
                    previewLabel = "Select Message";

                ImGui::Text("Message");
                ImGui::SameLine();
                ImGui::PushID("MessageCombo");
                ImGui::SetNextItemWidth(-80);
                if (readOnly) ImGui::BeginDisabled(true);
                if (ImGui::BeginCombo("##message", previewLabel.c_str()))
                {
                    if (ImGui::Selectable("None", calls[i].GetMessageName().empty()))
                    {
                        if (!readOnly)
                            calls[i].SetMessageName("");
                    }

                    for (const auto& option : options)
                    {
                        bool selected = (calls[i].GetMessageName() == option.messageName) &&
                            (calls[i].GetTargetMUID() == option.targetMUID.ToString());
                        if (ImGui::Selectable(option.label.c_str(), selected))
                        {
                            if (!readOnly)
                            {
                                calls[i].SetMessageName(option.messageName);
                                calls[i].SetTargetMUID(option.targetMUID.ToString());
                            }
                        }
                        if (selected)
                            ImGui::SetItemDefaultFocus();
                    }

                    ImGui::EndCombo();
                }
                if (readOnly) ImGui::EndDisabled();
                ImGui::PopID();

                ImGui::SameLine();
                if (!readOnly && ImGui::Button(u8"삭제"))
                {
                    calls.erase(calls.begin() + static_cast<std::ptrdiff_t>(i));
                    --i;
                }

                ImGui::PopID();
            }
            if (!readOnly && ImGui::Button(u8"+ 리스너 추가"))
            {
                calls.emplace_back("", "");
            }
            ImGui::Unindent(ImGui::GetTreeNodeToLabelSpacing());
        }
        ImGui::Unindent(ImGui::GetTreeNodeToLabelSpacing());
    };

    if (propType == rttr::type::get<SerializableEvent>())
    {
        SerializableEvent ev = var.get_value<SerializableEvent>();
        std::vector<PersistentCall> calls = ev.GetCalls();
        drawCalls(calls, false);
        ev.SetCalls(std::move(calls));
        prop.set_value(inst, ev);
        return true;
    }
    if (propType == rttr::type::get<SerializableEventT<float>>())
    {
        SerializableEventT<float> ev = var.get_value<SerializableEventT<float>>();
        std::vector<PersistentCall> calls = ev.GetCalls();
        drawCalls(calls, true);
        ev.SetCalls(std::move(calls));
        prop.set_value(inst, ev);
        return true;
    }

    return false;
}

static void DrawSequentialProperty_UnityLike(const std::string& name,
    rttr::variant& containerVar,
    const rttr::property& prop,
    rttr::instance inst,
    bool readOnly)
{
    auto view = containerVar.create_sequential_view();
    if (!view.is_valid()) return;

    const bool dynamic = view.is_dynamic();
    rttr::type elemType = view.get_value_type();

    // todo : 나중에 왼쪽 정렬로 만들어버리기 ( 커서 직접 옮기고 헤더 폭 알아서 줄이기 )

    // 1. 프로퍼티 헤더를 오른쪽으로 밀기
    ImGui::Indent(ImGui::GetTreeNodeToLabelSpacing());

    // 2. ID 충돌 방지: 레이블 뒤에 ##을 붙여 ID를 고정합니다.
    // 사이즈가 변해도 name이 같으면 열림 상태가 유지됩니다.
    std::string headerLabel = name + "  [" + std::to_string(view.get_size()) + "]###" + name;

    bool opened = ImGui::CollapsingHeader(headerLabel.c_str(), ImGuiTreeNodeFlags_DefaultOpen);

    if (opened)
    {
        // 내부 아이템 들여쓰기
        ImGui::Indent(ImGui::GetTreeNodeToLabelSpacing());

        // --- Size UI ---
        int size = (int)view.get_size();
        int newSize = size;
        bool sizeEditable = (!readOnly && dynamic);

        if (!sizeEditable) ImGui::BeginDisabled(true);
        ImGui::SetNextItemWidth(120);
        // Size 필드에도 고유 ID 부여
        if (ImGui::InputInt(("Size##" + name).c_str(), &newSize))
        {
            if (sizeEditable)
            {
                newSize = std::max(0, newSize);
                view.set_size((size_t)newSize);
            }
        }
        if (!sizeEditable) ImGui::EndDisabled();

        // --- Elements Loop ---
        int idx = 0;
        for (auto it = view.begin(); it != view.end();)
        {
            ImGui::PushID(idx);

            rttr::variant elem = (*it);
            rttr::variant unwrapped = elem.extract_wrapped_value();
            if (unwrapped.is_valid()) elem = unwrapped;

            std::string label = "Element " + std::to_string(idx);

            rttr::variant edited = elem;
            bool changed = false;
            (void)DrawSimplePropertyValue(label.c_str(), edited, elemType, readOnly, &changed, nullptr, rttr::instance());

            if (changed && !readOnly)
            {
                view.set_value((size_t)idx, edited);
            }

            ++it;
            ++idx;
            ImGui::PopID();
        }

        ImGui::Unindent(ImGui::GetTreeNodeToLabelSpacing());
    }

    // 3. 들여쓰기 복구
    ImGui::Unindent(ImGui::GetTreeNodeToLabelSpacing());

    if (!readOnly)
        prop.set_value(inst, containerVar);
}

static std::string MakeStringKey(const rttr::instance& inst, const rttr::property& prop)
{
    // 간단히: 타입명 + 프로퍼티명 (인스턴스별 구분까지 하려면 주소/ID 추가 권장)
    auto tname = inst.get_derived_type().get_name().to_string();
    auto pname = prop.get_name().to_string();
    return tname + "::" + pname;
}

void AddComponentFromDropFilePath(std::string filePath)
{
    if (!filePath.empty())
    {
        std::string ext = Utility::StringHelper::ExtractFileFormat(filePath);
        if (ext != "cpp" && ext != "h")
            return;

        std::string typeName = Utility::StringHelper::ExtractFileName(filePath);
        rttr::type t = rttr::type::get_by_name(typeName);

        if (t.is_valid())
        {
            g_selectedGameObject->AddComponent(t);
        }
    }
}

void MMMEngine::Editor::InspectorWindow::ClearCache()
{
    m_lastSelected = nullptr;
    m_componentTypes.clear(); // 중요: 유저 스크립트 타입 참조 해제
    m_pendingRemoveComponents.clear();
    m_stringEditCache.clear();
    // 필요하다면 rttr::type::get_invalid() 등을 활용해 더 확실히 비움
}

void MMMEngine::Editor::InspectorWindow::RenderProperties(rttr::instance inst, ObjPtr<Object> objPtr)
{
    static ObjPtr<GameObject> s_lastCachedObject = nullptr;
    static std::unordered_map<std::string, std::string> cache;
    if (s_lastCachedObject != g_selectedGameObject)
    {
        static std::unordered_map<std::string, std::string> emptyCache;
        cache.swap(emptyCache); // 캐시 클리어
        s_lastCachedObject = g_selectedGameObject;
    }

    auto t = inst.get_derived_type();
    rttr::property p = t.get_property("MUID");
    if (p.is_valid())
    {
        auto v = p.get_value(inst);
        if (v.is_valid() && v.is_type<MUID>())
            ImGui::PushID(v.get_value<MUID>().ToStringWithoutHyphens().c_str());
        else
            ImGui::PushID(inst.try_convert<void*>()); // 주소 기반(예시)
    }
    else
    {
        ImGui::PushID(inst.try_convert<void*>());
    }

    bool lockRefResolution = false;
    bool lockRectSize = false;
    Canvas* rectCanvas = nullptr;
    DirectX::SimpleMath::Vector2 rectCanvasSize = { 0.0f, 0.0f };
    std::string tname = t.get_name().to_string();
    if (t == rttr::type::get<RectTransform>())
    {
        auto rectPtr = ObjectManager::Get().GetPtr<RectTransform>(objPtr.GetPtrID(), objPtr.GetPtrGeneration());
        lockRectSize = IsScreenSpaceCanvasRoot(rectPtr->GetGameObject(), rectCanvas);
        if (lockRectSize && rectCanvas)
        {
            rectCanvasSize = rectCanvas->GetCanvasSize();
        }
    }
    else if (t == rttr::type::get<Canvas>())
    {
        auto canvasPtr = ObjectManager::Get().GetPtr<Canvas>(objPtr.GetPtrID(), objPtr.GetPtrGeneration());
        lockRefResolution = canvasPtr->GetScaleMode() == CanvasScaleMode::ConstantPixelSize ? true : false;
    }

    const std::unordered_map<std::string, bool> chainVisibility = BuildInspectorChainVisibility(inst, t);

    int propIndex = 0;
    for (auto& prop : t.get_properties())
    {
        rttr::variant md = prop.get_metadata("INSPECTOR");
        if (md.is_valid() && md.is_type<std::string>() && "HIDDEN" == md.get_value<std::string>())
            continue;

        rttr::variant var = prop.get_value(inst);
        if (!var.is_valid())
            continue;

        const std::string name = prop.get_name().to_string();
        auto chainIt = chainVisibility.find(name);
        if (chainIt != chainVisibility.end() && !chainIt->second)
            continue;
        bool readOnly = prop.is_readonly();
        rttr::type propType = prop.get_type();

        ImGui::PushID(propIndex++);

        if (lockRectSize && (name == "Width" || name == "Height" || name == "SizeDelta"))
            readOnly = true;

        if (lockRefResolution && name == "ReferenceResolution")
            readOnly = true;

        if (lockRectSize && name == "Width")
            var = rectCanvasSize.x;
        else if (lockRectSize && name == "Height")
            var = rectCanvasSize.y;

        if (DrawSerializableEventProperty(name, var, propType, prop, inst, readOnly))
        {
            ImGui::PopID();
            continue;
        }
        if (var.is_sequential_container())
        {
            DrawSequentialProperty_UnityLike(name, var, prop, inst, readOnly);
            ImGui::PopID();
            continue;
        }
        bool simpleChanged = false;
        if (DrawSimplePropertyValue(name.c_str(), var, propType, readOnly, &simpleChanged, &prop, inst))
        {
            if (simpleChanged && !readOnly)
            {
                prop.set_value(inst, var);
                if (t == rttr::type::get<Transform>() && name == "Position")
                    ApplyRigidBodyFromTransformIfPlaying(g_selectedGameObject);
            }
            ImGui::PopID();
            continue;
        }

        if (propType.get_name().to_string().find("ObjPtr") != std::string::npos)
        {
            MMMEngine::Object* obj = nullptr;
            std::string refName = "nullptr";

            if (var.convert(obj) && obj != nullptr)
            {
                if (auto compConvert = dynamic_cast<Component*>(obj))
                {
                    if (compConvert->GetGameObject().IsValid())
                        refName = compConvert->GetGameObject()->GetName();
                    else
                        refName = "Destroyed GameObject";
                }
                else
                {
                    if (auto goConvert = dynamic_cast<GameObject*>(obj))
                        refName = goConvert->GetName();
                    else
                        refName = name;
                }
            }

            // 프로퍼티 이름
            std::string ptrPropType = propType.get_name().to_string() + " " + prop.get_name().to_string();
            ptrPropType = std::regex_replace(ptrPropType, std::regex("ObjPtr"), "");
            ImGui::Text("%s:", ptrPropType.c_str());
            ImGui::SameLine();
            // 경로 표시 (클릭 가능하게)
            ImGui::PushID(name.c_str());
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.2f, 0.3f, 1.0f));
            ImGui::Button(refName.c_str(), ImVec2(-1, 0));
            ImGui::PopStyleColor();


            if (ImGui::BeginPopupContextItem("ObjPtrContext"))
            {
                if (!readOnly && ImGui::MenuItem(u8"참조 해제"))
                {
                    const ObjPtrBase& baseRef = ObjPtr<Object>{};
                    auto func = propType.get_method("Inject");
                    if (func.is_valid())
                    {
                        auto fvar = func.invoke(var, baseRef);
                        if (fvar.is_valid() && fvar.is_type<bool>() && fvar.get_value<bool>())
                        {
                            prop.set_value(inst, var);
                        }
                    }
                }
                ImGui::EndPopup();
            }
            ImGui::PopID();

            //MUID dragged_muid = GetMuid("gameobject_muid");
            Utility::MUID result = Utility::MUID::Empty();

            if (ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("gameobject_muid"))
                {
                    if (payload->IsDelivery() && payload->Data && payload->DataSize == sizeof(Utility::MUID))
                    {
                        std::memcpy(&result, payload->Data, sizeof(Utility::MUID));
                    }
                }
                ImGui::EndDragDropTarget();
            }

            MUID dragged_muid = result;
            if(dragged_muid.IsValid())
            {
                auto sceneRef = SceneManager::Get().GetCurrentScene();
                auto dragged = SceneManager::Get().FindWithMUID(sceneRef, dragged_muid);

                auto innertype = StringHelper::ExtractInnerTypeName(propType.get_name().to_string());

                if (innertype == "GameObject")
                {
                    const ObjPtrBase& baseRef = dragged;
                    auto func = propType.get_method("Inject");
                    if (func.is_valid())
                    {
                        auto fvar = func.invoke(var, baseRef);
                        if (fvar.is_valid() && fvar.is_type<bool>() && fvar.get_value<bool>())
                        {
                            prop.set_value(inst, var);
                            break;
                        }
                    }
                }
                else
                {
                    for (auto& _comp : dragged->GetAllComponents())
                    {
                        const ObjPtrBase& baseRef = _comp;
                        auto func = propType.get_method("Inject");
                        if (func.is_valid())
                        {
                            auto fvar = func.invoke(var, baseRef);
                            if (fvar.is_valid() && fvar.is_type<bool>() && fvar.get_value<bool>())
                            {
                                prop.set_value(inst, var);
                                break;
                            }
                        }
                    }
                }
            }
        }
        else if (propType.get_name().to_string().find("shared_ptr") != std::string::npos)
        {
            auto args = propType.get_template_arguments();
            if (args.begin() != args.end())
            {
                rttr::type innerType = *args.begin();

                if (innerType.is_derived_from(rttr::type::get<Resource>()) ||
                    innerType == rttr::type::get<Resource>())
                {
                    // 현재 리소스
                    Resource* res = nullptr;
                    auto sharedRes = var.get_value<std::shared_ptr<Resource>>();
                    if (sharedRes)
                        res = sharedRes.get();

                    // 표시용 경로
                    std::string displayPath = "None";
                    if (res)
                    {
                        std::wstring fullPath = res->GetFilePath();
                        if (!fullPath.empty())
                        {
                            displayPath = ProjectManager::Get().ToProjectRelativePath(
                                StringHelper::WStringToString(fullPath)
                            );
                        }
                    }

                    // 프로퍼티 이름
                    ImGui::Text("%s:", name.c_str());
                    ImGui::SameLine();
                    // 경로 표시 (클릭 가능하게)
                    ImGui::PushID(name.c_str());
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.2f, 0.3f, 1.0f));
                    ImGui::Button(displayPath.c_str(), ImVec2(-1, 0));
                    ImGui::PopStyleColor();

                    if (ImGui::BeginPopupContextItem("ResPtrContext"))
                    {
                        if (!readOnly && ImGui::MenuItem(u8"참조 해제"))
                        {
                            std::shared_ptr<Resource> emptyPtr = nullptr;
                            rttr::variant nullVar(emptyPtr);

                            if (nullVar.convert(prop.get_type()))
                            {
                                bool ok = prop.set_value(inst, nullVar);
                            }
                        }
                        ImGui::EndPopup();
                    }
                    // Drag & Drop
                    if (ImGui::BeginDragDropTarget())
                    {
                        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("FILE_PATH"))
                        {
                            std::string absolutePath((const char*)payload->Data, payload->DataSize - 1);

                            // 파일 확장자 검증 (선택사항)
                            std::string ext = std::filesystem::path(absolutePath).extension().string();

                            // TODO: 리소스 타입별 허용 확장자 체크
                            // 예: StaticMesh -> .mesh, Texture -> .png/.jpg 등

                            std::string relativePath = ProjectManager::Get().ToProjectRelativePath(absolutePath);
                            std::wstring wRelativePath = StringHelper::StringToWString(relativePath);

                            try
                            {
                                rttr::variant loadedResource = ResourceManager::Get().Load(innerType, wRelativePath);

                                if (loadedResource.is_valid())
                                {
                                    if (!readOnly)
                                    {
                                        if (loadedResource.convert(prop.get_type()))                  // v를 내부적으로 target type으로 변환 (bool 리턴)
                                        {
                                            prop.set_value(inst, loadedResource);                     // v는 이제 shared_ptr<StaticMesh> 타입 variant
                                        }
                                        else
                                        {
                                            // 변환 실패 처리
                                        }

                                    }
                                }
                            }
                            catch (const std::exception& e)
                            {
                                auto toUtf8 = [](const char* ansi) -> std::string
                                    {
                                        if (!ansi)
                                            return {};

                                        int wideLen = MultiByteToWideChar(CP_ACP, 0, ansi, -1, nullptr, 0);
                                        if (wideLen <= 0)
                                            return {};

                                        std::wstring wide(wideLen, L'\0');
                                        MultiByteToWideChar(CP_ACP, 0, ansi, -1, wide.data(), wideLen);

                                        int utf8Len = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
                                        std::string utf8(utf8Len, '\0');
                                        WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, utf8.data(), utf8Len, nullptr, nullptr);

                                        return utf8;
                                    };

                                // TODO: 에러 로그
                                std::cerr << "[Resource Load Error]\n"
                                    << "Type: " << innerType.get_name().to_string() << "\n"
                                    << "Path: " << StringHelper::WStringToString(wRelativePath) << "\n"
                                    << "What: " << toUtf8(e.what()) << std::endl;
                            }
                        }
                        ImGui::EndDragDropTarget();
                    }
                    ImGui::PopID();
                }
            }
        }
        else if (var.is_type<std::string>())
        {
            // MUID 가져오기
            rttr::property muidProp = t.get_property("MUID");
            rttr::variant muidVar = muidProp.get_value(inst);
            std::string muidStr = "unknown";
            if (muidVar.is_valid() && muidVar.is_type<MMMEngine::Utility::MUID>())
            {
                muidStr = muidVar.get_value<MMMEngine::Utility::MUID>().ToStringWithoutHyphens();
            }

            // 고유 키: MUID + 타입명 + 프로퍼티명
            std::string key = muidStr + "::" + inst.get_type().get_name().to_string() + "::" + name;

            std::string& editing = cache[key];

            if (editing.empty())
                editing = var.get_value<std::string>();

            char buf[256];
            strcpy_s(buf, editing.c_str());

            if (readOnly) ImGui::BeginDisabled(true);
            bool changed = ImGui::InputText(name.c_str(), buf, IM_ARRAYSIZE(buf));
            if (readOnly) ImGui::EndDisabled();

            if (changed && !readOnly)
            {
                editing = buf;
                prop.set_value(inst, editing);
            }
        }
        else if (var.is_type<Quaternion>())
        {
            auto SnapToZero = [](float& v, float eps = 1e-4f) {
                if (fabsf(v) < eps) v = 0.0f;
                };
            // 고유 키 (string 캐시처럼)
            rttr::property muidProp = t.get_property("MUID");
            rttr::variant muidVar = muidProp.get_value(inst);
            std::string muidStr = (muidVar.is_valid() && muidVar.is_type<MMMEngine::Utility::MUID>())
                ? muidVar.get_value<MMMEngine::Utility::MUID>().ToStringWithoutHyphens()
                : "unknown";

            std::string key = muidStr + "::" + inst.get_type().get_name().to_string() + "::" + name;

            // Quaternion용 캐시를 별도로 두는게 깔끔 (static map)
            static std::unordered_map<std::string, Vector3> eulerCache;

            // 1) 실제 값에서 Euler 계산 (도 단위)
            auto q = var.get_value<Quaternion>();
            Vector3 eRad = q.ToEuler(); // (SimpleMath는 보통 rad 반환)
            Vector3 eDeg = { eRad.x * (180.f / XM_PI), eRad.y * (180.f / XM_PI), eRad.z * (180.f / XM_PI) };

            // 2) 캐시 없으면 초기화
            if (eulerCache.find(key) == eulerCache.end())
                eulerCache[key] = eDeg;

            // 3) 인스펙터에서 "현재 이 항목을 편집 중"인지 판별하려면
            //    DragFloat3 호출 후 IsItemActive를 볼 수 있으니,
            //    호출 전에는 "이전 프레임의 상태"가 없어서 보통 이렇게 처리합니다:
            //    - 일단 data를 캐시로 세팅
            //    - DragFloat3 호출 후, Active가 아니고 changed도 아니면 실제 값으로 캐시를 동기화
            float data[3] = { eulerCache[key].x, eulerCache[key].y, eulerCache[key].z };


            if (readOnly) ImGui::BeginDisabled(true);
            bool changed = ImGui::DragFloat3(name.c_str(), data, 0.1f);
            bool active = ImGui::IsItemActive();
            if (readOnly) ImGui::EndDisabled();

            // 4) 사용자가 편집하지 않는 동안엔 gizmo 등 외부 변경을 반영
            if (!active && !changed)
            {
                SnapToZero(eDeg.x);
                SnapToZero(eDeg.y);
                SnapToZero(eDeg.z);
                eulerCache[key] = eDeg; // 외부 변경 반영(= gizmo 최신화)
            }

            // 5) 사용자가 인스펙터에서 편집한 경우만 set_value
            if (changed && !readOnly)
            {

                SnapToZero(data[0]);
                SnapToZero(data[1]);
                SnapToZero(data[2]);

                eulerCache[key] = { data[0], data[1], data[2] };

                Quaternion updatedQ = Quaternion::CreateFromYawPitchRoll(
                    eulerCache[key].y * (XM_PI / 180.f),
                    eulerCache[key].x * (XM_PI / 180.f),
                    eulerCache[key].z * (XM_PI / 180.f)
                );



                updatedQ.Normalize();
                prop.set_value(inst, updatedQ);
                if (t == rttr::type::get<Transform>() && name == "Rotation")
                    ApplyRigidBodyFromTransformIfPlaying(g_selectedGameObject);
            }
        }
        ImGui::PopID();
    }
    ImGui::PopID();
}

void MMMEngine::Editor::InspectorWindow::RefreshComponentTypes()
{
    m_componentTypes.clear();

    rttr::type componentType = rttr::type::get<Component>();

    for (const rttr::type& t : rttr::type::get_types())
    {
        // 포인터/기본형/컨테이너 등은 제외하고, "클래스/구조체(record)"만
        if (!t.is_class())
            continue;

        rttr::variant md = t.get_metadata("INSPECTOR");
        if (md.is_valid() && md.is_type<std::string>() && "DONT_ADD_COMP" == md.get_value<std::string>())
            continue;

        if (t != componentType && t.is_derived_from(componentType))
        {
            m_componentTypes.push_back(t);
        }
    }
}

void MMMEngine::Editor::InspectorWindow::Render()
{
	if (!g_editor_window_inspector)
		return;

	ImGuiWindowClass wc;
	// 핵심: 메인 뷰포트에 이 윈도우를 종속시킵니다.
	// 이렇게 하면 메인 창을 클릭해도 이 창이 '메인 창의 일부'로서 취급되어 우선순위를 가집니다.
	wc.ParentViewportId = ImGui::GetMainViewport()->ID;
	wc.ViewportFlagsOverrideSet = ImGuiViewportFlags_NoFocusOnAppearing; // 필요 시 설정

	ImGui::SetNextWindowClass(&wc);

	ImGuiStyle& style = ImGui::GetStyle();
	style.WindowMenuButtonPosition = ImGuiDir_None;


	ImGui::Begin(u8"\uf002 인스펙터", &g_editor_window_inspector);

    // 1. 선택된 게임 오브젝트가 있는지 확인
    if (g_selectedGameObject.IsValid())
    {
        // 2. 오브젝트 이름 출력 및 활성화 상태 체크박스
        char buf[256];
        strcpy_s(buf, g_selectedGameObject->GetName().c_str());
        if (ImGui::InputText("##ObjName", buf, IM_ARRAYSIZE(buf)))
        {
            g_selectedGameObject->SetName(buf);
        }

        ImGui::SameLine();

        bool isActive = g_selectedGameObject->IsActiveSelf();

        if (ImGui::Checkbox(u8"활성화" /* u8 활성화 */, &isActive))
        {
            g_selectedGameObject->SetActive(isActive);
        }

        char buf2[256];
        strcpy_s(buf2, g_selectedGameObject->GetTag().c_str());
        ImGui::SetNextItemWidth(150);
        if (ImGui::InputText(u8"태그" /* u8 태그 */, buf2, IM_ARRAYSIZE(buf2)))
        {
            g_selectedGameObject->SetTag(buf2);
        }

        ImGui::SameLine();

        // 현재 오브젝트 레이어 (0~31 가정)
        int curLayer = g_selectedGameObject->GetLayer();   // 없으면 멤버/저장값 쓰세요
        curLayer = (curLayer < 0) ? 0 : (curLayer > 31 ? 31 : curLayer);

        // 미리보기 텍스트
        char preview[8];
        sprintf_s(preview, "%d", curLayer);

        // 폭 지정
        ImGui::SetNextItemWidth(53);

        if (ImGui::BeginCombo(u8"레이어" /* u8 레이어 */, preview))
        {
            for (int n = 0; n <= 31; ++n)
            {
                bool selected = (n == curLayer);
                if (ImGui::Selectable(std::to_string(n).c_str(), selected))
                {
                    // 선택하는 순간 즉시 반영
                    g_selectedGameObject->SetLayer(n);
                    curLayer = n;
                }
                if (selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }


        ImGui::Separator();

        m_pendingRemoveComponents.clear();

        // 3. 모든 컴포넌트 순회 및 렌더링
        auto& components = g_selectedGameObject->GetAllComponents();
        int compCount = 0;
        for (auto& comp : components)
        {
            // 각 컴포넌트의 데이터를 ImGui로 출력
            bool visible = true;

            std::string typeName = comp->get_type().get_name().to_string();

           // auto ss = comp->get_type();

            std::string duplicatePrevantName = typeName + "##" + std::to_string(compCount++);
            if (typeName != "Transform")
            {
                if(ImGui::CollapsingHeader(duplicatePrevantName.c_str(), &visible, ImGuiTreeNodeFlags_DefaultOpen))
                    RenderProperties(*comp, comp.Cast<Object>());
            }
            else if (ImGui::CollapsingHeader(duplicatePrevantName.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
                RenderProperties(*comp, comp.Cast<Object>());

            if (!visible)
            {
                visible = true;
                m_pendingRemoveComponents.push_back(comp);
            }
        }

        for (auto& comp : m_pendingRemoveComponents)
        {
            Object::Destroy(comp);
        }

        ImGui::Separator();



        // add component popup
        float width = ImGui::GetContentRegionAvail().x;
        if (ImGui::Button(u8"컴포넌트 추가", ImVec2{ width, 0 }))
        {
            ImGui::OpenPopup(u8"컴포넌트 선택");
        }

        // add component popup
        static char searchBuffer[256] = "";
        int selectedIndex = -1;

        if (ImGui::BeginPopup(u8"컴포넌트 선택"))
        {
            RefreshComponentTypes();

            // 검색 입력 필드
            ImGui::SetNextItemWidth(-1);
            if (ImGui::IsWindowAppearing())
            {
                ImGui::SetKeyboardFocusHere();
                searchBuffer[0] = '\0'; // 팝업 열릴 때마다 검색어 초기화
            }
            ImGui::InputTextWithHint("##search", u8"검색...", searchBuffer, IM_ARRAYSIZE(searchBuffer));

            ImGui::Separator();

            // 스크롤 영역 (최대 8개 항목 높이)
            const float itemHeight = ImGui::GetTextLineHeightWithSpacing();
            const float maxHeight = itemHeight * 8;

            ImGui::BeginChild("ComponentList", ImVec2(300, maxHeight), false);

            std::string searchStr = searchBuffer;
            std::transform(searchStr.begin(), searchStr.end(), searchStr.begin(), ::tolower);

            for (int i = 0; i < m_componentTypes.size(); ++i)
            {
                auto type = m_componentTypes[i];
                std::string typeName = type.get_name().to_string();

                // 검색 필터링
                if (searchStr.length() > 0)
                {
                    std::string lowerTypeName = typeName;
                    std::transform(lowerTypeName.begin(), lowerTypeName.end(), lowerTypeName.begin(), ::tolower);

                    if (lowerTypeName.find(searchStr) == std::string::npos)
                        continue;
                }

                if (ImGui::Selectable(typeName.c_str()))
                {
                    selectedIndex = i;
                    auto selected = m_componentTypes[selectedIndex];
                    g_selectedGameObject->AddComponent(type);
                    ImGui::CloseCurrentPopup();
                }
            }

            ImGui::EndChild();
            ImGui::EndPopup();
        }

        // script drag and drop area
        ImGui::InvisibleButton("##", ImGui::GetContentRegionAvail());
        std::string file = Editor::GetString("FILE_PATH");
        AddComponentFromDropFilePath(file);
    }
    else
    {
        ImGui::Text(u8"선택된 오브젝트가 없습니다.");
    }

	ImGui::End();
}


