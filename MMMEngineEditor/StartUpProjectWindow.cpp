#define NOMINMAX
#include "StartUpProjectWindow.h"

#include "ProjectManager.h"
#include "EditorRegistry.h"

#include <windows.h>
#include <shobjidl.h>   // IFileDialog
#include <filesystem>
#include <cstdlib>

#pragma comment(lib, "Ole32.lib")

namespace fs = std::filesystem;

namespace
{
    // UTF-16(wstring) -> UTF-8(string)
    static std::string WideToUtf8(const std::wstring& w)
    {
        if (w.empty()) return {};
        int len = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
        std::string s(len, '\0');
        WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), s.data(), len, nullptr, nullptr);
        return s;
    }

    static bool PickProjectFolder(std::wstring& outFolder)
    {
        outFolder.clear();

        HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        bool didInit = SUCCEEDED(hr);

        IFileDialog* dlg = nullptr;
        hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dlg));
        if (FAILED(hr))
        {
            if (didInit) CoUninitialize();
            return false;
        }

        DWORD opts = 0;
        dlg->GetOptions(&opts);
        dlg->SetOptions(opts | FOS_FORCEFILESYSTEM | FOS_PICKFOLDERS);

        hr = dlg->Show(nullptr);
        if (SUCCEEDED(hr))
        {
            IShellItem* item = nullptr;
            if (SUCCEEDED(dlg->GetResult(&item)))
            {
                PWSTR path = nullptr;
                if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path)))
                {
                    outFolder = path;
                    CoTaskMemFree(path);
                }
                item->Release();
            }
        }

        dlg->Release();
        if (didInit) CoUninitialize();
        return !outFolder.empty();
    }

    static bool PickFolder(std::wstring& outFolder)
    {
        outFolder.clear();

        HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        bool didInit = SUCCEEDED(hr);

        IFileDialog* dlg = nullptr;
        hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dlg));
        if (FAILED(hr))
        {
            if (didInit) CoUninitialize();
            return false;
        }

        DWORD opts = 0;
        dlg->GetOptions(&opts);
        dlg->SetOptions(opts | FOS_FORCEFILESYSTEM | FOS_PICKFOLDERS);

        hr = dlg->Show(nullptr);
        if (SUCCEEDED(hr))
        {
            IShellItem* item = nullptr;
            if (SUCCEEDED(dlg->GetResult(&item)))
            {
                PWSTR path = nullptr;
                if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path)))
                {
                    outFolder = path;
                    CoTaskMemFree(path);
                }
                item->Release();
            }
        }

        dlg->Release();
        if (didInit) CoUninitialize();
        return !outFolder.empty();
    }

    static bool HasEngineDir()
    {
        const char* engineDir = std::getenv("MMMENGINE_DIR");
        return (engineDir != nullptr && engineDir[0] != '\0');
    }
}

namespace MMMEngine::Editor
{
    void StartUpProjectWindow::RenderMissingEngineDirPopup()
    {
        if (m_openMissingEngineDirPopup)
        {
            ImGui::OpenPopup(u8"엔진 경로 설정 필요");
            m_openMissingEngineDirPopup = false;
        }

        if (ImGui::BeginPopupModal(u8"엔진 경로 설정 필요", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::TextWrapped(u8"프로젝트 생성을 위해 엔진 경로 환경변수가 필요합니다.\n"
                u8"환경 변수를 설정한 후 에디터를 재시작해주세요.");
            ImGui::Separator();

            ImGui::TextWrapped(u8"환경 변수 이름:");
            {
                char buf[256]{};
                strncpy_s(buf, "MMMENGINE_DIR", _TRUNCATE);
                ImGui::InputText("##envname", buf, sizeof(buf),
                    ImGuiInputTextFlags_ReadOnly | ImGuiInputTextFlags_AutoSelectAll);
            }
            ImGui::SameLine();
            if (ImGui::Button(u8"복사##envname"))
                ImGui::SetClipboardText("MMMENGINE_DIR");

            ImGui::Spacing();

            ImGui::TextWrapped(u8"예시 값 (엔진 루트 경로):");
            {
                char buf[256]{};
                strncpy_s(buf, "D:/Dev/MMMEngine", _TRUNCATE);
                ImGui::InputText("##envexample", buf, sizeof(buf),
                    ImGuiInputTextFlags_ReadOnly | ImGuiInputTextFlags_AutoSelectAll);
            }
            ImGui::SameLine();
            if (ImGui::Button(u8"복사##envexample"))
                ImGui::SetClipboardText("D:/Dev/MMMEngine");

            ImGui::Spacing();
            ImGui::TextDisabled(u8"참고: EngineShared/Include, EngineShared/Lib/Win64가 있는 경로");

            ImGui::Spacing();

            auto hbuttonsize = ImVec2{ ImGui::GetContentRegionAvail().x / 2 - ImGui::GetStyle().ItemSpacing.x / 2, 0 };

            auto currentCursorX = ImGui::GetCursorPosX();
            ImGui::SetCursorPosX(currentCursorX + (hbuttonsize.x / 2));
            if (ImGui::Button(u8"닫기", hbuttonsize))
                ImGui::CloseCurrentPopup();

            ImGui::EndPopup();
        }
    }

    void StartUpProjectWindow::ShowError(const char* msg)
    {
        m_errorMsg = msg ? msg : "";
        m_openErrorPopup = true;
    }

    void StartUpProjectWindow::RenderErrorPopup()
    {
        if (m_openErrorPopup)
        {
            ImGui::OpenPopup(u8"Project Load Error");
            m_openErrorPopup = false;
        }

        if (ImGui::BeginPopupModal(u8"Project Load Error", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::TextWrapped(u8"%s", m_errorMsg.c_str());
            ImGui::Spacing();
            if (ImGui::Button(u8"OK", ImVec2(120, 0)))
                ImGui::CloseCurrentPopup();

            ImGui::EndPopup();
        }
    }

    void StartUpProjectWindow::Render()
    {
        ImGuiWindowClass wc;
        wc.ParentViewportId = ImGui::GetMainViewport()->ID;
        wc.ViewportFlagsOverrideSet = ImGuiViewportFlags_NoFocusOnAppearing;
        ImGui::SetNextWindowClass(&wc);

        ImGui::SetNextWindowSize(ImVec2(510, 185), ImGuiCond_Once);
        if (!ImGui::Begin(u8"프로젝트 스타트업", nullptr,
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking))
        {
            ImGui::End();
            return;
        }

        auto hbuttonsize = ImVec2{ ImGui::GetContentRegionAvail().x / 2 - ImGui::GetStyle().ItemSpacing.x / 2, ImGui::GetContentRegionAvail().y / 2 };

        // ---- Open existing project ----
        if (ImGui::Button(u8"기존 프로젝트 폴더 열기##open", hbuttonsize))
        {
            PickProjectFolder(m_selectedProjectFolder);
            if (m_selectedProjectFolder.empty())
            {
                ShowError(u8"프로젝트 폴더를 선택하지 않았습니다.");
            }
            else
            {
                fs::path projectRoot = fs::path(m_selectedProjectFolder);
                fs::path projectSettingsDir = projectRoot / "ProjectSettings";

                // ProjectSettings 폴더 존재 여부 확인
                if (!fs::exists(projectSettingsDir) || !fs::is_directory(projectSettingsDir))
                {
                    ShowError(u8"선택한 폴더는 올바른 에디터 프로젝트 폴더가 아닙니다.\n"
                        u8"ProjectSettings 폴더를 찾을 수 없습니다.");
                }
                else
                {
                    fs::path projectJsonPath = projectSettingsDir / "project.json";

                    // project.json 존재 여부에 따라 처리
                    if (fs::exists(projectJsonPath))
                    {
                        // 기존 프로젝트 열기 (OpenProject에서 rootPath 업데이트)
                        if (ProjectManager::Get().OpenProject(projectJsonPath))
                        {
                            EditorRegistry::g_editor_project_loaded = true;
                        }
                        else
                        {
                            ShowError(u8"프로젝트 파일을 읽을 수 없습니다.\n"
                                u8"project.json 파일이 손상되었을 수 있습니다.");
                        }
                    }
                    else
                    {
                        // project.json 생성 후 프로젝트 열기
                        if (ProjectManager::Get().CreateNewProject(projectRoot))
                        {
                            EditorRegistry::g_editor_project_loaded = true;
                        }
                        else
                        {
                            ShowError(u8"프로젝트 파일 생성에 실패했습니다.\n"
                                u8"폴더 권한을 확인하고 다시 시도하세요.");
                        }
                    }
                }
            }
        }

        ImGui::SameLine();

        // ---- Create new project ----
        if (ImGui::Button(u8"지정 경로에 새 프로젝트 생성##createbtn", hbuttonsize))
        {
            PickFolder(m_selectedProjectRoot);
            if (m_selectedProjectRoot.empty())
            {
                ShowError(u8"프로젝트 루트 폴더를 먼저 선택하세요.");
            }
            else
            {
                // 환경변수 체크
                if (!HasEngineDir())
                {
                    m_openMissingEngineDirPopup = true;
                }
                else
                {
                    fs::path root = fs::path(m_selectedProjectRoot);
                    if (ProjectManager::Get().CreateNewProject(root))
                    {
                        EditorRegistry::g_editor_project_loaded = true;
                    }
                    else
                    {
                        ShowError(u8"새 프로젝트 생성에 실패했습니다.\n폴더 권한/경로를 확인하고 다시 시도하세요.");
                    }
                }
            }
        }

        ImGui::Separator();
        ImGui::TextDisabled(u8"프로젝트 로드 성공 시 자동으로 에디터가 프로젝트 모드로 전환됩니다.");

        // 팝업은 End 전에 호출
        RenderErrorPopup();
        RenderMissingEngineDirPopup();

        ImGui::End();
    }
}
