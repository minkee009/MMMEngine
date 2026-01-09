#pragma once
#include "Singleton.hpp"
#include "MMMInput.h"
#include <Windows.h>
#include <map>

namespace MMMEngine
{
    class InputManager : public Singleton<InputManager>
    {
    private:
        HWND m_hWnd; // 윈도우 핸들
        POINT m_mouseClient; // 마우스 좌표
        SHORT m_prevState[256] = { 0 };
        SHORT m_currState[256] = { 0 };

        RECT m_clientRect;

        std::map<Input::KeyCode, int> m_keyCodeMap; // KeyCode와 Windows VKey 매핑
        void InitKeyCodeMap(); // KeyCode를 Windows VKey로 매핑

        //// 게임패드 관리 (최대 4개)
        //std::array<XInputGamepadDevice*, 4> m_gamepads;
        //void InitGamepads(); // 게임패드 초기화
        //void CleanupGamepads(); // 게임패드 정리


        Vector2 GetMousePos();
        bool GetKey(Input::KeyCode keyCode);
        bool GetKeyDown(Input::KeyCode keyCode);
        bool GetKeyUp(Input::KeyCode keyCode);
        int GetNativeKeyCode(Input::KeyCode keyCode) const;

        friend Vector2 Input::GetMousePos();
        friend bool Input::GetKey(Input::KeyCode keyCode);
        friend bool Input::GetKeyDown(Input::KeyCode keyCode);
        friend bool Input::GetKeyUp(Input::KeyCode keyCode);
    public:
        InputManager() = default;
        ~InputManager() = default;

        void StartUp(HANDLE windowHandle);
        void ShutDown();
        void Update();
    };
}