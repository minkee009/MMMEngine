#pragma once
#include "Singleton.hpp"
#include "BuildManager.h"
#include <filesystem>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>

namespace MMMEngine::Editor
{
    class PlayerBuildWindow : public Utility::Singleton<PlayerBuildWindow>
    {
    public:
        virtual ~PlayerBuildWindow();
        // UI 렌더링
        void Render();

    private:
        // 빌드 시작
        void StartBuild();

        // 폴더 선택 다이얼로그
        bool SelectOutputFolder(std::filesystem::path& outPath);

        // 빌드 설정 UI
        void RenderBuildSettings();

        // 빌드 진행 상황 UI
        void RenderBuildProgress();

        // 상태
        bool m_needClear = true;
        std::atomic<bool> m_building{ false };
        std::atomic<float> m_progress{ 0.f };
        std::atomic<int> m_exitCode{ 0 };

        std::thread m_worker;

        // 빌드 설정
        BuildConfiguration m_buildConfig = BuildConfiguration::Release;
        std::filesystem::path m_outputPath;
        std::string m_executableName = "MMMEnginePlayer";  // 확장자 제외한 이름

        // 로그 출력
        std::string m_currentMessage;
        std::string m_buildLog;

        // UI 상태
        bool m_showSuccessToast = false;
        double m_toastHideTime = 0.0;

        // 자동 스크롤
        bool m_autoScroll = true;

        // 로그 동기화
        mutable std::mutex m_logMutex;
    };
}
