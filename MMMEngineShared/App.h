#pragma once
#include "Export.h"
#include <Windows.h>
#include <string>

#include "Delegates.hpp"
#include "Resolution.h"
#include "DisplayMode.h"

#pragma warning(push)
#pragma warning(disable: 4251)

namespace MMMEngine::Utility
{
	/// <summary>
	/// 윈도우 하나의 대한 핸들과 루프를 제공합니다.
	/// </summary>
	class MMMENGINE_API App
	{
	public:
		struct WindowInfo
		{
			std::wstring title;
			LONG width;
			LONG height;
			DWORD style;
		};

		struct WindowedRestore
		{
			DWORD style = 0;
			DWORD exStyle = 0;
			RECT  rect = { 0,0,0,0 };
			bool  valid = false;
		};

		App();
		App(HINSTANCE hInstance);
		App(LPCWSTR title, LONG width, LONG height);
		App(HINSTANCE hInstance,LPCWSTR title, LONG width, LONG height);

		~App();

		int Run();
		void Quit();

		void ToggleFullscreen(); // Windowed <-> Fullscreen 토글

		Event<App, void(void)> OnInitialize{ this };
		Event<App, void(void)> OnRelease{ this };
		Event<App, void(void)> OnUpdate{ this };
		Event<App, void(void)> OnRender{ this };
		Event<App, void(int,int)> OnWindowSizeChanged{ this };
		Event<App, void(HWND, UINT, WPARAM, LPARAM)> OnBeforeWindowMessage{ this };
		Event<App, void(HWND, UINT, WPARAM, LPARAM)> OnAfterWindowMessage{ this };
		Event<App, void(WPARAM, LPARAM)> OnMouseWheelUpdate{ this };

		DisplayMode GetDisplayMode() const;

		const WindowInfo GetWindowInfo() const;
		HWND GetWindowHandle() const;

		std::vector<Resolution> GetCurrentMonitorResolutions() const;
		static std::vector<Resolution> GetMonitorResolutionsFromWindow(HWND hWnd);

		void SetWindowTitle(const std::wstring& title);
		void SetDisplayMode(DisplayMode mode);

		void SetWindowSize(int width, int height);
		void SetResizable(bool isResizable);
	protected:
		LRESULT HandleWindowMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	private:
		bool m_isRunning;
		WindowInfo m_windowInfo;
		WindowedRestore m_windowedRestore;
		DisplayMode m_currentDisplayMode;
		DisplayMode m_previousDisplayMode;
		bool m_isResizable = true;

		bool m_windowSizeDirty;

		HINSTANCE m_hInstance;
		HWND m_hWnd;


		bool CreateMainWindow();
		void SetWindowed();
		void SetBorderlessWindowed();
		void SetFullscreen();
		void SaveWindowedState();
		void RestoreWindowedState();
		static LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	};
}

#pragma warning(pop)