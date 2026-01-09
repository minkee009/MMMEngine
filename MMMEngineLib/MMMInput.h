#pragma once
#include <functional>
#include "SimpleMath.h"

using namespace DirectX::SimpleMath;
namespace MMMEngine::Input
{
    constexpr uint32_t KEY_PRESSED = 0x8000;
    constexpr uint32_t MAX_GAMEPADS = 4;

    enum class KeyCode
    {
        // 알파벳
        A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,

        // 숫자
        Alpha0, Alpha1, Alpha2, Alpha3, Alpha4, Alpha5, Alpha6, Alpha7, Alpha8, Alpha9,

        // 특수 키
        Escape, Space, Enter, Tab, Backspace, Delete,
        LeftShift, RightShift, LeftControl, RightControl, LeftAlt, RightAlt,
        UpArrow, DownArrow, LeftArrow, RightArrow,

        Comma, Period, Slash, Semicolon, Quote, LeftBracket, RightBracket, Minus, Equals,

        F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,

        // 마우스 버튼
        MouseLeft, MouseRight, MouseMiddle,

        // 기타
        Unknown,
    };

    // === 키보드 및 마우스 입력 ===
    Vector2 GetMousePos();
    bool GetKey(KeyCode keyCode);
    bool GetKeyDown(KeyCode keyCode);
    bool GetKeyUp(KeyCode keyCode);

    //// 게임패드 버튼 (플랫폼 독립적)
    //enum class GamepadButton
    //{
    //    ButtonSouth = 0,  // A / Cross
    //    ButtonEast,       // B / Circle  
    //    ButtonNorth,      // Y / Triangle
    //    ButtonWest,       // X / Square
    //    ButtonL1,         // Left Shoulder
    //    ButtonR1,         // Right Shoulder
    //    ButtonStart,      // Start/Options/Menu
    //    ButtonSelect,     // Back/Share/View
    //    DPadUp,           // D-Pad 위
    //    DPadDown,         // D-Pad 아래
    //    DPadLeft,         // D-Pad 왼쪽
    //    DPadRight,        // D-Pad 오른쪽
    //    Count
    //};

    //// 게임패드 축 (플랫폼 독립적)
    //enum class GamepadAxis
    //{
    //    LeftStickX = 0,
    //    LeftStickY,
    //    RightStickX,
    //    RightStickY,
    //    LeftTrigger,
    //    RightTrigger,
    //    DPadX,          // D-Pad 가로축 (-1: 왼쪽, 0: 중립, +1: 오른쪽)
    //    DPadY,          // D-Pad 세로축 (-1: 아래, 0: 중립, +1: 위)
    //    Count
    //};


    //// === 게임패드 입력 ===
    //// 버튼 입력
    //bool GetGamepadButton(int gamepadIndex, GamepadButton button);
    //bool GetGamepadButtonDown(int gamepadIndex, GamepadButton button);
    //bool GetGamepadButtonUp(int gamepadIndex, GamepadButton button);

    //// 아날로그 축 입력
    //float GetGamepadAxis(int gamepadIndex, GamepadAxis axis);
    //float GetGamepadAxisRaw(int gamepadIdex, GamepadAxis axis);
    //Vector2 GetLeftStick(int gamepadIndex);   // 왼쪽 스틱 (X, Y)
    //Vector2 GetRightStick(int gamepadIndex);  // 오른쪽 스틱 (X, Y)
    //float GetLeftTrigger(int gamepadIndex);   // 왼쪽 트리거 (0.0f ~ 1.0f)
    //float GetRightTrigger(int gamepadIndex);  // 오른쪽 트리거 (0.0f ~ 1.0f)

    //// D-Pad 편의 함수들
    //Vector2 GetDPad(int gamepadIndex);           // D-Pad 축 (X, Y)
    //float GetDPadX(int gamepadIndex);            // D-Pad 가로축 (-1, 0, +1)
    //float GetDPadY(int gamepadIndex);            // D-Pad 세로축 (-1, 0, +1)

    //// === 게임패드 상태 정보 ===
    //bool IsGamepadConnected(int gamepadIndex);
    //int GetConnectedGamepadCount();
    //int GetFirstConnectedGamepad();

    //// === 핫플러그 지원 ===
    //void SetGamepadConnectionCallback(GamepadConnectionCallback callback);
    //bool WasGamepadJustConnected(int gamepadIndex);
    //bool WasGamepadJustDisconnected(int gamepadIndex);
    //void ClearGamepadConnectionEvents();

    //// === 게임패드 진동 기능 ===
    //void SetGamepadVibration(int gamepadIndex, float leftMotor, float rightMotor);
    //void StopGamepadVibration(int gamepadIndex);
    //void PlaySimpleGamepadVibration(int gamepadIndex, float duration, float strength);

    //GamepadConnectionCallback g_connectionCallback;

    //// 게임패드 연결/해제 이벤트 콜백
    //using GamepadConnectionCallback = std::function<void(int gamepadIndex, bool connected)>;

}