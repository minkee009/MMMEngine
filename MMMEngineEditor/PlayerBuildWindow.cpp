#include "PlayerBuildWindow.h"
#include "ProjectManager.h"
#include "EditorRegistry.h"
#include "imgui.h"
#include <Windows.h>
#include <shobjidl.h>
#include <sstream>

namespace fs = std::filesystem;
using namespace MMMEngine::EditorRegistry;

namespace MMMEngine::Editor
{
    PlayerBuildWindow::~PlayerBuildWindow()
    {
        if (m_worker.joinable())
            m_worker.join();
    }

    bool PlayerBuildWindow::SelectOutputFolder(fs::path& outPath)
    {
        HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
        bool needUninit = SUCCEEDED(hr);

        IFileDialog* dlg = nullptr;
        hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dlg));

        if (FAILED(hr))
        {
            if (needUninit) CoUninitialize();
            return false;
        }

        // 폴더 선택 모드 설정
        DWORD dwOptions;
        hr = dlg->GetOptions(&dwOptions);
        if (SUCCEEDED(hr))
        {
            dlg->SetOptions(dwOptions | FOS_PICKFOLDERS | FOS_PATHMUSTEXIST);
        }

        // 기본 폴더 설정 (프로젝트 루트)
        if (ProjectManager::Get().HasActiveProject())
        {
            auto projectRoot = ProjectManager::Get().GetActiveProject().ProjectRootFS();
            IShellItem* psiFolder = nullptr;
            hr = SHCreateItemFromParsingName(projectRoot.c_str(), nullptr, IID_PPV_ARGS(&psiFolder));
            if (SUCCEEDED(hr))
            {
                dlg->SetFolder(psiFolder);
                psiFolder->Release();
            }
        }

        // 다이얼로그 표시
        hr = dlg->Show(nullptr);

        bool success = false;
        if (SUCCEEDED(hr))
        {
            IShellItem* pItem = nullptr;
            hr = dlg->GetResult(&pItem);
            if (SUCCEEDED(hr))
            {
                PWSTR pszPath = nullptr;
                hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszPath);
                if (SUCCEEDED(hr))
                {
                    outPath = fs::path(pszPath);
                    CoTaskMemFree(pszPath);
                    success = true;
                }
                pItem->Release();
            }
        }

        dlg->Release();
        if (needUninit) CoUninitialize();

        return success;
    }

    void PlayerBuildWindow::StartBuild()
    {
        if (m_building.exchange(true))
            return;

        // 출력 경로 유효성 검사
        if (m_outputPath.empty())
        {
            m_buildLog += u8"[오류] 출력 경로를 선택해주세요.\n";
            m_building.store(false);
            return;
        }

        // 실행 파일 이름 유효성 검사
        if (m_executableName.empty())
        {
            m_buildLog += u8"[오류] 실행 파일 이름을 입력해주세요.\n";
            m_building.store(false);
            return;
        }

        // 프로젝트 확인
        if (!ProjectManager::Get().HasActiveProject())
        {
            m_buildLog += u8"[오류] 활성 프로젝트가 없습니다.\n";
            m_building.store(false);
            return;
        }

        m_progress.store(0.f);
        m_exitCode.store(0);
        m_buildLog.clear();
        m_currentMessage.clear();
        m_showSuccessToast = false;

        // 이전 스레드 정리
        if (m_worker.joinable())
            m_worker.join();

        m_worker = std::thread([this]()
            {
                auto& bm = BuildManager::Get();

                // 진행률 콜백 설정
                bm.SetProgressCallbackPercent([this](float p)
                    {
                        if (p < 0.f) p = 0.f;
                        if (p > 100.f) p = 100.f;
                        m_progress.store(p);
                    });

                // 로그 콜백 설정
                bm.SetProgressCallbackString([this](const std::string& msg)
                    {
                        m_currentMessage = msg;

                        // 로그 누적
                        std::lock_guard<std::mutex> lock(m_logMutex);
                        m_buildLog += msg;
                        if (!msg.empty() && msg.back() != '\n')
                            m_buildLog += '\n';
                    });

                // 프로젝트 루트 가져오기
                const auto& project = ProjectManager::Get().GetActiveProject();
                fs::path projectRoot = project.ProjectRootFS();

                // 플레이어 빌드 실행 (사용자 지정 이름 전달)
                auto output = bm.BuildPlayer(projectRoot, m_outputPath, m_buildConfig, m_executableName);

                m_exitCode.store(output.exitCode);
                m_building.store(false);

                // 완료 후 토스트 표시
                if (output.result == BuildResult::Success)
                {
                    m_showSuccessToast = true;
                    m_toastHideTime = ImGui::GetTime() + 3.0; // 3초간 표시
                }
                else
                {
                    // 실패 시 에러 로그 추가
                    std::lock_guard<std::mutex> lock(m_logMutex);
                    m_buildLog += u8"\n[오류] 빌드 실패:\n";
                    m_buildLog += output.errorLog;
                    m_buildLog += '\n';
                }
            });
    }

    void PlayerBuildWindow::RenderBuildSettings()
    {
        ImGui::SeparatorText(u8"빌드 설정");

        // 빌드 구성 선택
        const char* configs[] = { "Debug", "Release" };
        int currentConfig = (m_buildConfig == BuildConfiguration::Debug) ? 0 : 1;

        if (ImGui::Combo(u8"빌드 구성", &currentConfig, configs, IM_ARRAYSIZE(configs)))
        {
            m_buildConfig = (currentConfig == 0) ? BuildConfiguration::Debug : BuildConfiguration::Release;
        }

        ImGui::Spacing();

        // 실행 파일 이름 입력
        ImGui::Text(u8"실행 파일 이름:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(250);

        char buffer[256];
        strncpy_s(buffer, m_executableName.c_str(), sizeof(buffer) - 1);
        buffer[sizeof(buffer) - 1] = '\0';

        if (ImGui::InputText("##ExeName", buffer, sizeof(buffer)))
        {
            m_executableName = buffer;
        }

        ImGui::SameLine();
        ImGui::TextDisabled(".exe");

        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip(u8"최종 실행 파일의 이름을 지정합니다 (확장자 제외)");
        }

        ImGui::Spacing();

        // 출력 경로 설정
        ImGui::Text(u8"출력 경로:");
        ImGui::SameLine();

        std::string displayPath = m_outputPath.empty()
            ? u8"(선택되지 않음)"
            : m_outputPath.string();

        ImGui::TextWrapped("%s", displayPath.c_str());

        if (ImGui::Button(u8"출력 폴더 선택...", ImVec2(200, 0)))
        {
            fs::path selectedPath;
            if (SelectOutputFolder(selectedPath))
            {
                m_outputPath = selectedPath;
            }
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // 빌드 버튼
        bool canBuild = !m_building.load() && !m_outputPath.empty() && !m_executableName.empty();

        if (!canBuild)
            ImGui::BeginDisabled();

        if (ImGui::Button(u8"플레이어 빌드 시작", ImVec2(200, 40)))
        {
            StartBuild();
        }

        if (!canBuild)
            ImGui::EndDisabled();

        // 빌드 중이면 취소 버튼 표시
        if (m_building.load())
        {
            ImGui::SameLine();
            if (ImGui::Button(u8"취소 (구현 예정)", ImVec2(150, 40)))
            {
                // TODO: 빌드 취소 기능 구현
            }
        }
    }

    void PlayerBuildWindow::RenderBuildProgress()
    {
        ImGui::SeparatorText(u8"빌드 진행 상황");

        // 진행률 표시
        float progress = m_progress.load();
        char progressText[64];
        snprintf(progressText, sizeof(progressText), "%.1f%%", progress);

        ImGui::ProgressBar(progress / 100.f, ImVec2(-1, 0), progressText);

        // 현재 작업 표시
        if (m_building.load() && !m_currentMessage.empty())
        {
            ImGui::Spacing();
            ImGui::TextWrapped(u8"현재 작업: %s", m_currentMessage.c_str());
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // 빌드 로그
        ImGui::Text(u8"빌드 로그:");
        ImGui::SameLine();
        ImGui::Checkbox(u8"자동 스크롤", &m_autoScroll);

        ImGui::BeginChild("BuildLog", ImVec2(0, 300), true, ImGuiWindowFlags_HorizontalScrollbar);
        {
            std::lock_guard<std::mutex> lock(m_logMutex);
            ImGui::TextUnformatted(m_buildLog.c_str());

            // 자동 스크롤
            if (m_autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
                ImGui::SetScrollHereY(1.0f);
        }
        ImGui::EndChild();

        // 로그 클리어 버튼
        if (ImGui::Button(u8"로그 지우기"))
        {
            std::lock_guard<std::mutex> lock(m_logMutex);
            m_buildLog.clear();
        }

        ImGui::SameLine();

        // 출력 폴더 열기 버튼
        if (!m_outputPath.empty())
        {
            if (ImGui::Button(u8"출력 폴더 열기"))
            {
                std::string command = "explorer \"" + m_outputPath.string() + "\"";
                system(command.c_str());
            }
        }
    }
    void PlayerBuildWindow::Render()
    {
        // 스레드 정리
        if (!m_building.load() && m_worker.joinable())
        {
            m_worker.join();
        }

        // 성공 토스트 렌더링
        if (m_showSuccessToast && ImGui::GetTime() < m_toastHideTime)
        {
            ImGuiWindowFlags flags =
                ImGuiWindowFlags_NoDecoration |
                ImGuiWindowFlags_AlwaysAutoResize |
                ImGuiWindowFlags_NoSavedSettings |
                ImGuiWindowFlags_NoFocusOnAppearing |
                ImGuiWindowFlags_NoNav |
                ImGuiWindowFlags_NoMove;

            ImGuiViewport* vp = ImGui::GetMainViewport();
            ImVec2 center(
                vp->WorkPos.x + vp->WorkSize.x * 0.5f,
                vp->WorkPos.y + vp->WorkSize.y * 0.5f
            );

            ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
            ImGui::SetNextWindowBgAlpha(0.85f);

            if (ImGui::Begin("##PlayerBuildSuccessToast", nullptr, flags))
            {
                ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), u8"✓ 플레이어 빌드 성공!");
                ImGui::Spacing();
                ImGui::TextWrapped(u8"출력 위치: %s", m_outputPath.string().c_str());
            }
            ImGui::End();
        }
        else if (m_showSuccessToast)
        {
            m_showSuccessToast = false;
        }

        // 메인 윈도우
        ImGuiWindowClass wc;
        wc.ParentViewportId = ImGui::GetMainViewport()->ID;
        wc.ViewportFlagsOverrideSet = ImGuiViewportFlags_NoFocusOnAppearing;
        ImGui::SetNextWindowClass(&wc);
        if (!g_editor_window_playerBuild)
            return;

        ImGui::SetNextWindowSize(ImVec2(700, 600), ImGuiCond_FirstUseEver);

        if (m_needClear)
        {
			m_needClear = false;
            m_buildLog.clear();
            m_currentMessage.clear();
            m_progress.store(0.f);
            m_exitCode.store(0);
        }

        if (ImGui::Begin(u8"플레이어 빌드", &g_editor_window_playerBuild, ImGuiWindowFlags_None))
        {
            // 프로젝트 정보 표시
            if (ProjectManager::Get().HasActiveProject())
            {
                const auto& project = ProjectManager::Get().GetActiveProject();
                ImGui::Text(u8"프로젝트: %s", project.ProjectRootFS().filename().string().c_str());
                ImGui::Separator();
                ImGui::Spacing();
            }

            // 빌드 설정 UI
            RenderBuildSettings();

            ImGui::Spacing();

            // 빌드 진행 상황 UI
            RenderBuildProgress();
        }
        ImGui::End();

        // 창이 닫히려고 할 때 체크
        if (!g_editor_window_playerBuild)
        {
			m_needClear = true;
        }
    }
}
