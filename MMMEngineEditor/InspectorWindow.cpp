#include "InspectorWindow.h"
#include "SceneManager.h"
#include "Transform.h"

#include "EditorRegistry.h"
using namespace MMMEngine::EditorRegistry;
using namespace DirectX::SimpleMath;
using namespace DirectX;

#include <optional>

using namespace MMMEngine;
using namespace MMMEngine::Utility;

static Vector3 g_eulerCache;
static ObjPtr<GameObject> g_lastSelected = nullptr;

void RenderProperties(rttr::instance inst)
{
    // 인스턴스의 타입 정보를 가져옴
    rttr::type t = inst.get_derived_type();

    for (auto& prop : t.get_properties())
    {
        rttr::variant var = prop.get_value(inst);
        const std::string& name = prop.get_name().to_string();

        // 타입에 따른 ImGui 렌더링 분기
        if (var.is_type<Vector3>())
        {
            Vector3 v = var.get_value<Vector3>();
            float data[3] = { v.x, v.y, v.z };
            if (ImGui::DragFloat3(name.c_str(), data, 0.1f))
            {
                prop.set_value(inst, Vector3(data[0], data[1], data[2]));
            }
        }
        else if (var.is_type<float>())
        {
            float f = var.get_value<float>();
            if (ImGui::DragFloat(name.c_str(), &f))
            {
                prop.set_value(inst, f);
            }
        }
        else if (var.is_type<Quaternion>())
        {
            if (g_lastSelected != g_selectedGameObject)
            {
                Quaternion q = var.get_value<Quaternion>();
                Vector3 e = q.ToEuler();
                g_eulerCache = { e.x * (180.f / XM_PI), e.y * (180.f / XM_PI), e.z * (180.f / XM_PI) };
                g_lastSelected = g_selectedGameObject;
            }

            // 1. 실제 데이터 대신 '캐시된 오일러 값'을 UI에 표시
            float data[3] = { g_eulerCache.x, g_eulerCache.y, g_eulerCache.z };

            if (ImGui::DragFloat3(name.c_str(), data, 0.1f))
            {
                // 2. 캐시 업데이트
                g_eulerCache = { data[0], data[1], data[2] };

                // 3. 캐시 -> 쿼터니언 변환 후 적용 (역변환 과정이 없어서 90도에서도 안전함)
                Quaternion updatedQ = Quaternion::CreateFromYawPitchRoll(
                    g_eulerCache.y * (XM_PI / 180.f),
                    g_eulerCache.x * (XM_PI / 180.f),
                    g_eulerCache.z * (XM_PI / 180.f)
                );

                prop.set_value(inst, updatedQ);
            }
        }
        // 추가적인 타입들(int, bool, string 등)에 대한 처리...
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

	ImGui::Begin(u8"인스펙터", &g_editor_window_inspector);

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

        if (ImGui::Checkbox(u8"활성화", &isActive))
        {
            g_selectedGameObject->SetActive(isActive);
        }

        ImGui::Separator();

        // 3. 모든 컴포넌트 순회 및 렌더링
        // GameObject에 GetComponent 리스트를 가져오는 함수가 있다고 가정합니다.
        auto& components = g_selectedGameObject->GetAllComponents();
        for (auto& comp : components)
        {
            // 각 컴포넌트의 데이터를 ImGui로 출력
            if (ImGui::CollapsingHeader(comp->get_type().get_name().to_string().c_str(), ImGuiTreeNodeFlags_DefaultOpen))
            {
                RenderProperties(*comp);
            }
        }

        ImGui::Separator();

        float width = ImGui::GetContentRegionAvail().x;
        if (ImGui::Button(u8"컴포넌트 추가", ImVec2{ width, 0 })) { /*ImGui.OpenPopup("ChooseComponent");*/ }
    }
    else
    {
        ImGui::Text(u8"선택된 오브젝트가 없습니다.");
    }

	ImGui::End();
}
