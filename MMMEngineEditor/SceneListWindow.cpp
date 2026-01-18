#include "SceneListWindow.h"
#include "EditorRegistry.h"
#include "StringHelper.h"
#include "SceneManager.h"
#include "SceneSerializer.h"
#include <algorithm>
using namespace MMMEngine::EditorRegistry;
using namespace MMMEngine;
using namespace MMMEngine::Utility;

struct sceneData
{
    bool enable;
    uint32_t idx;
    std::string name;
};

std::vector<sceneData> g_sceneList;
std::vector<sceneData> g_sceneList_original; // 원본 데이터 보관
bool g_hasChanges = false; // 변경사항 추적
bool g_showConfirmDialog = false; // 확인 다이얼로그 표시 여부
static char g_newSceneName[256] = "";
static bool g_showCreateDialog = false;
static bool g_showEmptyWarningDialog = false;
static bool g_showDuplicateNameDialog = false;
static char g_duplicateNameMsg[256] = "";


static bool IsSceneNameDuplicate(const char* name)
{
    if (!name) return false;

    for (const auto& s : g_sceneList)
    {
        if (s.name == name) // 완전 동일 비교
            return true;
    }
    return false;
}

void ShowDuplicateNameDialog()
{
    if (g_showDuplicateNameDialog)
    {
        ImGui::OpenPopup(u8"경고##warning_scenelist_01");

        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

        if (ImGui::BeginPopupModal(u8"경고##warning_scenelist_01", NULL, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Text(u8"이미 존재하는 씬 이름입니다.");
            if (g_duplicateNameMsg[0] != '\0')
                ImGui::Text("%s", g_duplicateNameMsg);
            ImGui::Separator();

            auto hbuttonsize = ImVec2{ ImGui::GetContentRegionAvail().x, 0 };
            if (ImGui::Button(u8"확인", hbuttonsize))
            {
                g_showDuplicateNameDialog = false;
                g_showCreateDialog = true;  // 생성 다이얼로그 다시 열기
                memset(g_duplicateNameMsg, 0, sizeof(g_duplicateNameMsg));
                // g_newSceneName은 유지해서 사용자가 수정할 수 있게 함
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
    }
}
void ShowEmptyWarningDialog()
{
    if (g_showEmptyWarningDialog)
    {
        ImGui::OpenPopup(u8"경고##warning_scenelist_02");

        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

        if (ImGui::BeginPopupModal(u8"경고##warning_scenelist_02", NULL, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Text(u8"씬 리스트가 비어있습니다.");
            ImGui::Text(u8"최소 하나 이상의 씬이 필요합니다.");
            ImGui::Separator();

            auto hbuttonsize = ImVec2{ ImGui::GetContentRegionAvail().x / 2 - ImGui::GetStyle().ItemSpacing.x / 2, 0 };
            auto getCurrentX = ImGui::GetCursorPosX();
            ImGui::SetCursorPosX(getCurrentX + (hbuttonsize.x / 2));
            if (ImGui::Button(u8"확인", hbuttonsize))
            {
                g_showEmptyWarningDialog = false;
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
    }
}

void ShowCreateSceneDialog()
{
    if (g_showCreateDialog)
    {
        ImGui::OpenPopup(u8"새 씬 생성");

        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

        if (ImGui::BeginPopupModal(u8"새 씬 생성", NULL, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Text(u8"새 씬의 이름을 입력하세요:");
            ImGui::Separator();

            ImGui::SetNextItemWidth(300.0f);
            ImGui::InputText("##SceneName", g_newSceneName, IM_ARRAYSIZE(g_newSceneName));

            ImGui::Separator();

            // 확인 버튼
            auto hbuttonsize = ImVec2{ ImGui::GetContentRegionAvail().x / 2 - ImGui::GetStyle().ItemSpacing.x / 2, 0 };

            if (ImGui::Button(u8"생성", hbuttonsize))
            {
                if (strlen(g_newSceneName) > 0)
                {
                    // 중복 체크
                    if (IsSceneNameDuplicate(g_newSceneName))
                    {
                        // **수정: 현재 팝업을 먼저 닫고 경고 팝업을 표시**
                        g_showDuplicateNameDialog = true;
                        snprintf(g_duplicateNameMsg, sizeof(g_duplicateNameMsg), u8"입력한 이름: %s", g_newSceneName);
                        g_showCreateDialog = false;  // 이 줄 추가
                        ImGui::CloseCurrentPopup();   // 이 줄 추가
                    }
                    else
                    {
                        sceneData newScene;
                        newScene.enable = true;
                        newScene.idx = g_sceneList.empty() ? 0 : g_sceneList.back().idx + 1;
                        newScene.name = g_newSceneName;

                        g_sceneList.push_back(newScene);
                        g_hasChanges = true;

                        memset(g_newSceneName, 0, sizeof(g_newSceneName));
                        g_showCreateDialog = false;
                        ImGui::CloseCurrentPopup();
                    }
                }
            }

            ImGui::SameLine();

            // 취소 버튼
            if (ImGui::Button(u8"취소", hbuttonsize))
            {
                // 입력 필드 초기화
                memset(g_newSceneName, 0, sizeof(g_newSceneName));
                g_showCreateDialog = false;
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
    }
}
// 변경사항 적용 함수
bool ApplySceneListChanges()
{
    bool hasEnabledScene = false;
    for (const auto& scene : g_sceneList)
    {
        if (scene.enable)
        {
            hasEnabledScene = true;
            break;
        }
    }

    if (!hasEnabledScene)
    {
        g_showEmptyWarningDialog = true;
        return false; // 실패
    }

    // 실제로 남은 것들을 Scene매니저에 전송
    auto it = std::remove_if(g_sceneList.begin(), g_sceneList.end(), [](const auto& scene) {
        return !scene.enable;
        });

    g_sceneList.erase(it, g_sceneList.end());

    // 이후 남은 것들만 enabledScenes에 담기
    std::vector<std::string> enabledScenes;
    for (const auto& scene : g_sceneList) {
        enabledScenes.push_back(scene.name);
    }

    g_sceneList_original = g_sceneList;
    g_hasChanges = false;

    // TODO: SceneManager 적용
    SceneManager::Get().UpdateAndReloadScenes(enabledScenes);
    SceneSerializer::Get().ExtractScenes(SceneManager::Get().GetAllSceneToRaw(), SceneManager::Get().GetSceneListPath());
    return true; // 성공
}

// 변경사항 취소 함수
void DiscardSceneListChanges()
{
    g_sceneList = g_sceneList_original; // 원본으로 되돌림
    g_hasChanges = false;
}

// 변경사항 확인 함수
void CheckForChanges()
{
    if (g_sceneList.size() != g_sceneList_original.size())
    {
        g_hasChanges = true;
        return;
    }

    for (int i = 0; i < g_sceneList.size(); i++)
    {
        if (g_sceneList[i].enable != g_sceneList_original[i].enable ||
            g_sceneList[i].idx != g_sceneList_original[i].idx)
        {
            g_hasChanges = true;
            return;
        }
    }

    g_hasChanges = false;
}

void MMMEngine::Editor::SceneListWindow::Render()
{
    ImGuiWindowClass wc;
    wc.ParentViewportId = ImGui::GetMainViewport()->ID;
    wc.ViewportFlagsOverrideSet = ImGuiViewportFlags_NoFocusOnAppearing;
    ImGui::SetNextWindowClass(&wc);
    if (!g_editor_window_scenelist)
        return;

    static bool firstLoad = true;
    if (firstLoad)
    {
        uint32_t idx = 0;
        for (auto& scene : SceneManager::Get().GetAllSceneToRaw())
        {
            g_sceneList.push_back({ true, idx++, scene->GetName() });
        }
        g_sceneList_original = g_sceneList; // 원본 저장
        firstLoad = false;
    }

    // 창 제목 설정 (변경사항 있으면 * 추가)
    std::string windowTitle = g_hasChanges ? u8"씬 리스트*###SceneList" : u8"씬 리스트###SceneList";

    ImGui::Begin(windowTitle.c_str(), &g_editor_window_scenelist, ImGuiWindowFlags_NoDocking);

    // 창이 닫히려고 할 때 체크
    if (!g_editor_window_scenelist && g_hasChanges && !g_showConfirmDialog)
    {
        g_showConfirmDialog = true;
        g_editor_window_scenelist = true; // 창을 다시 열어둠
    }

    // 현재 창의 사용 가능한 영역 크기 가져오기
    ImVec2 available_size = ImGui::GetContentRegionAvail();
    // 적용 버튼의 높이 (패딩 포함)
    float button_height = ImGui::GetFrameHeight();
    float spacing = ImGui::GetStyle().ItemSpacing.y;
    // 드래그 영역 크기 계산 (버튼 높이와 간격을 제외한 나머지 전체)
    ImVec2 drag_box_size(available_size.x, available_size.y - button_height - spacing);

    ImGui::BeginChild("DragBox", drag_box_size, true);

    // 드래그 앤 드롭으로 순서 변경
    int enableIdx = -1;
    for (int i = 0; i < g_sceneList.size(); i++)
    {
        auto& data = g_sceneList[i];
        if (data.enable)
            enableIdx++;
        std::string validIdxStr = (!data.enable) ? u8"비활성화" : std::to_string(enableIdx);
        std::string dataStr = validIdxStr + " - " + data.name;

        ImGui::PushID(i);

        // 체크박스
        if (ImGui::Checkbox(dataStr.c_str(), &data.enable))
        {
            CheckForChanges(); // 변경사항 확인
        }

        // 드래그 소스 설정
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
        {
            ImGui::SetDragDropPayload("SCENE_ITEM", &i, sizeof(int));
            ImGui::Text(u8"이동: %s", dataStr.c_str());
            ImGui::EndDragDropSource();
        }

        // 드롭 타겟 설정
        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("SCENE_ITEM"))
            {
                int payload_n = *(const int*)payload->Data;
                // 아이템 순서 교환
                if (payload_n != i)
                {
                    sceneData temp = g_sceneList[payload_n];
                    g_sceneList.erase(g_sceneList.begin() + payload_n);
                    g_sceneList.insert(g_sceneList.begin() + i, temp);
                    g_hasChanges = true; // 순서 변경 시 변경사항 표시
                }
            }
            ImGui::EndDragDropTarget();
        }

        ImGui::PopID();
    }

    auto hbuttonsize = ImVec2{ ImGui::GetContentRegionAvail().x / 2 - ImGui::GetStyle().ItemSpacing.x / 2, 0 };
    if (ImGui::Button(u8"불러오기", hbuttonsize))
    {
        // 불러오기 기능
    }

    ImGui::SameLine();

    // 새로 생성 버튼
    if (ImGui::Button(u8"새로 생성", hbuttonsize))
    {
        g_showCreateDialog = true;
    }
    ImGui::EndChild();

    // 적용 버튼 (창 너비만큼, 하단 고정)
    if (ImGui::Button(u8"적용", ImVec2(-1, 0)))
    {
        ApplySceneListChanges(); // 함수로 처리
    }
    ImGui::End();

    // 빈 리스트 경고 다이얼로그 렌더링
    ShowEmptyWarningDialog();

    // 새 씬 생성 다이얼로그 렌더링
    ShowCreateSceneDialog();

    ShowDuplicateNameDialog();

    // 확인 다이얼로그
    if (g_showConfirmDialog)
    {
        ImGui::OpenPopup(u8"변경사항 확인");

        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

        if (ImGui::BeginPopupModal(u8"변경사항 확인", NULL, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Text(u8"저장되지 않은 변경사항이 있습니다.");
            ImGui::Text(u8"적용하지 않고 닫으시겠습니까?");
            ImGui::Separator();

            if (ImGui::Button(u8"적용하고 닫기", ImVec2(120, 0)))
            {
                const bool ok = ApplySceneListChanges();

                // 핵심: 실패했으면 confirm을 끝내고 리스트로 돌아간다
                g_showConfirmDialog = false;

                if (ok)
                    g_editor_window_scenelist = false;

                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button(u8"적용하지 않고 닫기", ImVec2(150, 0)))
            {
                DiscardSceneListChanges(); // 함수로 처리
                g_showConfirmDialog = false;
                g_editor_window_scenelist = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button(u8"취소", ImVec2(120, 0)))
            {
                g_showConfirmDialog = false;
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
    }
}