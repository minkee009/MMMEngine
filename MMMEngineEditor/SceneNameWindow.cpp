#include "SceneNameWindow.h"
#include "SceneManager.h"
#include "SceneListWindow.h"
#include "EditorRegistry.h"
#include "json/json.hpp"

using namespace MMMEngine::EditorRegistry;
using json = nlohmann::json;

#include "ProjectManager.h"
#include "SceneSerializer.h"


namespace MMMEngine::Editor
{
	void SceneNameWindow::Render()
	{
		ImGuiViewport* vp = ImGui::GetMainViewport();
		ImVec2 center = vp->GetCenter();

		ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

		if (ImGui::BeginPopupModal(u8"씬 이름 변경 경고", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::Text(u8"씬 이름 변경 사항이 적용되지 않았습니다.\n적용하지 않고 창을 닫으시겠습니까?");
			ImGui::Separator();
			if (ImGui::Button(u8"예", ImVec2(120, 0)))
			{
				m_hasChanges = false;
				m_firstShowSceneName = true;
				g_editor_window_sceneName = false;
				ImGui::CloseCurrentPopup();
			}
			ImGui::SetItemDefaultFocus();
			ImGui::SameLine();
			if (ImGui::Button(u8"아니요", ImVec2(120, 0)))
			{
				ImGui::CloseCurrentPopup();
				g_editor_window_sceneName = true;
			}
			ImGui::EndPopup();
		}

        ImGuiWindowClass wc;
        wc.ParentViewportId = ImGui::GetMainViewport()->ID;
        wc.ViewportFlagsOverrideSet = ImGuiViewportFlags_NoFocusOnAppearing;
        ImGui::SetNextWindowClass(&wc);
        if (!g_editor_window_sceneName)
            return;

        // 창 제목 설정 (변경사항 있으면 * 추가)
        std::string windowTitle = m_hasChanges ? u8"씬 이름 변경*###SceneName" : u8"씬 이름 변경###SceneName";

		ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

		bool applyButtonEnabled = false;
		if(ImGui::Begin(windowTitle.c_str(), &g_editor_window_sceneName, ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_AlwaysAutoResize))
		{
			static char sceneNameBuffer[256] = "";

			auto sceneRef = SceneManager::Get().GetCurrentScene();
			auto sceneRaw = SceneManager::Get().GetSceneRaw(sceneRef);
			if (m_firstShowSceneName)
			{
				// 현재 씬 이름으로 초기화
				if (sceneRaw)
				{
					strncpy_s(sceneNameBuffer, sceneRaw->GetName().c_str(), sizeof(sceneNameBuffer));
				}

				m_firstShowSceneName = false;
			}

			ImGui::InputText(u8".scene", sceneNameBuffer, sizeof(sceneNameBuffer));

			// 변경사항 체크
			if (sceneRaw && std::string(sceneNameBuffer) != sceneRaw->GetName())
			{
				m_hasChanges = true;
			}
			else
			{
				m_hasChanges = false;
			}

			ImGui::SameLine();
			if (ImGui::Button(u8"적용"))
			{
				if (sceneRaw)
				{
					sceneRaw->SetName(std::string(sceneNameBuffer));
				}

				// 씬 리스트 갱신 후 씬 리스트 파일에서 이름 변경 적용
				SceneListWindow::Get().RefreshSceneList();
				SceneSerializer::Get().ExtractScenesList(SceneManager::Get().GetAllSceneToRaw(), SceneManager::Get().GetSceneListPath());

				// 씬 폴더 내 씬 스냅샷 파일 이름도 변경
				auto sceneListPath = SceneManager::Get().GetSceneListPath();

				ImGui::CloseCurrentPopup();
				applyButtonEnabled = true;
			}

			ImGui::End();
		}

		// 변경사항이 있지만 적용하지 않은 경우 창을 다시열고 경고창
		if (!g_editor_window_sceneName && m_hasChanges && !applyButtonEnabled)
		{
			g_editor_window_sceneName = true;
			ImGui::OpenPopup(u8"씬 이름 변경 경고");
		}
	}
}
