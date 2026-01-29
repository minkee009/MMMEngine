#pragma once
#include <imgui.h>
#include <string>
#include "Singleton.hpp"

namespace MMMEngine::Editor
{
    class StartUpProjectWindow : public Utility::Singleton<StartUpProjectWindow>
    {
    public:
        void Render();

    private:
        // UI state
        std::wstring m_selectedProjectFolder; // project.json 경로
        std::wstring m_selectedProjectRoot; // 새 프로젝트 루트 폴더

        // error popup state
        bool m_openErrorPopup = false;
        std::string m_errorMsg;

        std::wstring m_pendingCreateRoot; // 템플릿 설정 후 재시도할 루트

        void ShowError(const char* msg);
        void RenderErrorPopup();
        void RenderMissingEngineDirPopup();
        bool m_openMissingEngineDirPopup = false;
    };
}
