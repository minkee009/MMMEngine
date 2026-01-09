#pragma once
#include <Windows.h>
#include <string>

#include "Singleton.hpp"
#include "Delegates.hpp"

namespace MMMEngine
{
	class Application : public Singleton<Application>
	{
	public:
		struct WindowInfo
		{
			LPCWSTR title;
			LONG width;
			LONG height;
			LONG style;
		};

		Application();
		~Application();

		int Run();
		void Quit();

		Event<Application, void(void)> OnIntialize{ this };
		Event<Application, void(void)> OnShutdown{ this };
		Event<Application, void(void)> OnUpdate{ this };
		Event<Application, void(void)> OnRender{ this };
		Event<Application, void(int,int)> OnResize{ this };

		void SetProcessHandle(HINSTANCE hinstance);
		WindowInfo GetWindowInfo();
		HWND GetWindowHandle();
	protected:
		LRESULT HandleWindowMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	private:
		HINSTANCE m_hInstance;
		HWND m_hWnd;
		bool m_isRunning;

		bool m_needResizeWindow;

		WindowInfo m_windowInfo;

		bool CreateMainWindow();
		static LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	};
}