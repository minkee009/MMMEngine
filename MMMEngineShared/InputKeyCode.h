#pragma once
#include "Export.h"
#include <Windows.h>
#include <functional>
#include <utility>

namespace MMMEngine
{
    enum class KeyCode : unsigned char
    {
        // 알파벳
        A = 'A',
        B = 'B',
        C = 'C', 
        D = 'D', 
        E = 'E', 
        F = 'F', 
        G = 'G', 
        H = 'H', 
        I = 'I', 
        J = 'J', 
        K = 'K', 
        L = 'L', 
        M = 'M', 
        N = 'N', 
        O = 'O', 
        P = 'P', 
        Q = 'Q', 
        R = 'R', 
        S = 'S', 
        T = 'T', 
        U = 'U',
        V = 'V', 
        W = 'W', 
        X = 'X', 
        Y = 'Y', 
        Z = 'Z',

        // 숫자
        Alpha0 = '0', 
        Alpha1 = '1', 
        Alpha2 = '2', 
        Alpha3 = '3', 
        Alpha4 = '4', 
        Alpha5 = '5', 
        Alpha6 = '6', 
        Alpha7 = '7', 
        Alpha8 = '8', 
        Alpha9 = '9',

        // 특수 키
        Escape = VK_ESCAPE,
        Space = VK_SPACE,
        Enter = VK_RETURN,
        Tab = VK_TAB,
        Backspace = VK_BACK,
        Delete = VK_DELETE, 
        LeftShift = VK_LSHIFT,
        RightShift = VK_RSHIFT,
        LeftControl = VK_LCONTROL,
        RightControl = VK_RCONTROL,
        LeftAlt = VK_LMENU, 
        RightAlt = VK_RMENU,
        UpArrow = VK_UP,
        DownArrow = VK_DOWN,
        LeftArrow = VK_LEFT,
        RightArrow = VK_RIGHT,

        Comma = VK_OEM_COMMA,
        Period = VK_OEM_PERIOD,
        Slash = VK_OEM_2,
        Semicolon = VK_OEM_1,
        Quote = VK_OEM_7,
        LeftBracket = VK_OEM_4,
        RightBracket = VK_OEM_6,
        Minus = VK_OEM_MINUS,
        Equals = VK_OEM_PLUS,

        F1  = VK_F1,
        F2 = VK_F2,
        F3 = VK_F3,
        F4 = VK_F4,
        F5 = VK_F5,
        F6 = VK_F6,
        F7 = VK_F7,
        F8 = VK_F8,
        F9 = VK_F9,
        F10 = VK_F10,
        F11 = VK_F11,
        F12 = VK_F12,

        // 마우스 버튼
        MouseLeft = VK_LBUTTON,
        MouseRight = VK_RBUTTON,
        MouseMiddle = VK_MBUTTON,

        // 기타
        Unknown = 255U,
    };
}
