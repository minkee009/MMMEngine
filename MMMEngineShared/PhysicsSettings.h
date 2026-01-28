#pragma once
#include "ExportSingleton.hpp"
#include <filesystem>

namespace MMMEngine
{
	class MMMENGINE_API PhysicsSettings : public Utility::ExportSingleton<PhysicsSettings>
	{
	public:
		void StartUp(const std::filesystem::path& rootPath) {
			// 경로를 내부 변수에 고정 (외부에서 알 필요 없음)
			m_configFilePath = rootPath / SETTINGS_FILENAME;

			// 시작 시 자동 로드 및 엔진 적용
			if (std::filesystem::exists(m_configFilePath)) {
				LoadSettings();
				ApplySettings();
			}
			else {
				// 기본 레이어 이름 예시 (필요시 추가)
				idToName[0] = "Default";

				// 모든 레이어가 서로 충돌하도록 초기화 (ResetAll 정책)
				for (uint32_t i = 0; i < 32; ++i) {
					collisionMatrix[i] = 0xFFFFFFFFu;
				}

				// 즉시 저장하여 파일 생성
				SaveSettings();
				// 엔진에 적용
				ApplySettings();

				// 로그 시스템이 있다면 출력 (예: 기본 물리 설정 파일이 생성되었습니다.)
			}
		}

		bool SaveSettings();
		bool LoadSettings();
		void ApplySettings();

		std::map<uint32_t, std::string> idToName;
		uint32_t collisionMatrix[32] = { 0, };
	private:
		std::filesystem::path m_configFilePath;
		const std::string SETTINGS_FILENAME = "physics.settings";
	};
}
