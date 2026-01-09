#pragma once
#include "Singleton.hpp"
#include <chrono>

namespace MMMEngine
{
	class TimeManager : public Singleton<TimeManager>
	{
	private:
		enum class ExcutionContext
		{
			Normal, // 일반 업데이트
			Fixed  // 고정 업데이트
		};

		ExcutionContext m_executionContext = ExcutionContext::Normal;

		void SetExecutionContext(ExcutionContext context) { m_executionContext = context; }

		std::chrono::high_resolution_clock::time_point m_initTime;
		std::chrono::high_resolution_clock::time_point m_prevTime;
		std::chrono::high_resolution_clock::time_point m_currentTime;
		std::chrono::high_resolution_clock::duration m_deltaDuration;
		std::chrono::high_resolution_clock::duration m_totalDuration;

		float m_deltaTime = 0;
		float m_fixedDeltaTime = 0.03125f;
		float m_totalTime = 0;
		float m_fixedTime = 0;
		float m_maximumAllowedTimestep = 0.2f;
		float m_timeScale = 1.0f;
		float m_unscaledDeltaTime = 0;
		float m_unscaledTotalTime = 0;
		std::uint32_t m_frameCount = 0;

	public:
		void StartUp();
		void ShutDown();
		void Update();
		void FixedUpdate();

		float GetDeltaTime() const;
		float GetFixedDeltaTime() const { return m_fixedDeltaTime; }
		float GetTime() const;
		float GetFixedTime() const { return m_fixedTime; }
		float GetUnscaledTime() const { return m_unscaledTotalTime; }
		float GetUnscaledDeltaTime() const { return m_unscaledDeltaTime; }
		float GetTimeScale() const { return m_timeScale; }
		float GetMaximumAllowedTimestep() const { return m_maximumAllowedTimestep; }
		std::uint32_t GetFrameCount() const { return m_frameCount; }
	};
}