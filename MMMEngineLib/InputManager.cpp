#include "InputManager.h"
#include "TimeManager.h"

void MMMEngine::InputManager::InitKeyCodeMap()
{
    // KeyCode와 Windows Virtual Key Code 매핑
    m_keyCodeMap[Input::KeyCode::A] = 'A';
    m_keyCodeMap[Input::KeyCode::B] = 'B';
    m_keyCodeMap[Input::KeyCode::C] = 'C';
    m_keyCodeMap[Input::KeyCode::D] = 'D';
    m_keyCodeMap[Input::KeyCode::E] = 'E';
    m_keyCodeMap[Input::KeyCode::F] = 'F';
    m_keyCodeMap[Input::KeyCode::G] = 'G';
    m_keyCodeMap[Input::KeyCode::H] = 'H';
    m_keyCodeMap[Input::KeyCode::I] = 'I';
    m_keyCodeMap[Input::KeyCode::J] = 'J';
    m_keyCodeMap[Input::KeyCode::K] = 'K';
    m_keyCodeMap[Input::KeyCode::L] = 'L';
    m_keyCodeMap[Input::KeyCode::M] = 'M';
    m_keyCodeMap[Input::KeyCode::N] = 'N';
    m_keyCodeMap[Input::KeyCode::O] = 'O';
    m_keyCodeMap[Input::KeyCode::P] = 'P';
    m_keyCodeMap[Input::KeyCode::Q] = 'Q';
    m_keyCodeMap[Input::KeyCode::R] = 'R';
    m_keyCodeMap[Input::KeyCode::S] = 'S';
    m_keyCodeMap[Input::KeyCode::T] = 'T';
    m_keyCodeMap[Input::KeyCode::U] = 'U';
    m_keyCodeMap[Input::KeyCode::V] = 'V';
    m_keyCodeMap[Input::KeyCode::W] = 'W';
    m_keyCodeMap[Input::KeyCode::X] = 'X';
    m_keyCodeMap[Input::KeyCode::Y] = 'Y';
    m_keyCodeMap[Input::KeyCode::Z] = 'Z';

    m_keyCodeMap[Input::KeyCode::Alpha0] = '0';
    m_keyCodeMap[Input::KeyCode::Alpha1] = '1';
    m_keyCodeMap[Input::KeyCode::Alpha2] = '2';
    m_keyCodeMap[Input::KeyCode::Alpha3] = '3';
    m_keyCodeMap[Input::KeyCode::Alpha4] = '4';
    m_keyCodeMap[Input::KeyCode::Alpha5] = '5';
    m_keyCodeMap[Input::KeyCode::Alpha6] = '6';
    m_keyCodeMap[Input::KeyCode::Alpha7] = '7';
    m_keyCodeMap[Input::KeyCode::Alpha8] = '8';
    m_keyCodeMap[Input::KeyCode::Alpha9] = '9';

    m_keyCodeMap[Input::KeyCode::Escape] = VK_ESCAPE;
    m_keyCodeMap[Input::KeyCode::Space] = VK_SPACE;
    m_keyCodeMap[Input::KeyCode::Enter] = VK_RETURN;
    m_keyCodeMap[Input::KeyCode::Tab] = VK_TAB;
    m_keyCodeMap[Input::KeyCode::Backspace] = VK_BACK;
    m_keyCodeMap[Input::KeyCode::Delete] = VK_DELETE;

    m_keyCodeMap[Input::KeyCode::LeftShift] = VK_LSHIFT;
    m_keyCodeMap[Input::KeyCode::RightShift] = VK_RSHIFT;
    m_keyCodeMap[Input::KeyCode::LeftControl] = VK_LCONTROL;
    m_keyCodeMap[Input::KeyCode::RightControl] = VK_RCONTROL;
    m_keyCodeMap[Input::KeyCode::LeftAlt] = VK_LMENU; // VK_MENU는 Alt 키
    m_keyCodeMap[Input::KeyCode::RightAlt] = VK_RMENU;

    m_keyCodeMap[Input::KeyCode::UpArrow] = VK_UP;
    m_keyCodeMap[Input::KeyCode::DownArrow] = VK_DOWN;
    m_keyCodeMap[Input::KeyCode::LeftArrow] = VK_LEFT;
    m_keyCodeMap[Input::KeyCode::RightArrow] = VK_RIGHT;

    m_keyCodeMap[Input::KeyCode::Comma] = VK_OEM_COMMA;
    m_keyCodeMap[Input::KeyCode::Period] = VK_OEM_PERIOD;
    m_keyCodeMap[Input::KeyCode::Slash] = VK_OEM_2;
    m_keyCodeMap[Input::KeyCode::Semicolon] = VK_OEM_1;
    m_keyCodeMap[Input::KeyCode::Quote] = VK_OEM_7;
    m_keyCodeMap[Input::KeyCode::LeftBracket] = VK_OEM_4;
    m_keyCodeMap[Input::KeyCode::RightBracket] = VK_OEM_6;
    m_keyCodeMap[Input::KeyCode::Minus] = VK_OEM_MINUS;
    m_keyCodeMap[Input::KeyCode::Equals] = VK_OEM_PLUS;

    m_keyCodeMap[Input::KeyCode::F1] = VK_F1;
    m_keyCodeMap[Input::KeyCode::F2] = VK_F2;
    m_keyCodeMap[Input::KeyCode::F3] = VK_F3;
    m_keyCodeMap[Input::KeyCode::F4] = VK_F4;
    m_keyCodeMap[Input::KeyCode::F5] = VK_F5;
    m_keyCodeMap[Input::KeyCode::F6] = VK_F6;
    m_keyCodeMap[Input::KeyCode::F7] = VK_F7;
    m_keyCodeMap[Input::KeyCode::F8] = VK_F8;
    m_keyCodeMap[Input::KeyCode::F9] = VK_F9;
    m_keyCodeMap[Input::KeyCode::F10] = VK_F10;
    m_keyCodeMap[Input::KeyCode::F11] = VK_F11;
    m_keyCodeMap[Input::KeyCode::F12] = VK_F12;

    m_keyCodeMap[Input::KeyCode::MouseLeft] = VK_LBUTTON;
    m_keyCodeMap[Input::KeyCode::MouseRight] = VK_RBUTTON;
    m_keyCodeMap[Input::KeyCode::MouseMiddle] = VK_MBUTTON;
}

int MMMEngine::InputManager::GetNativeKeyCode(Input::KeyCode keyCode) const
{
    auto it = m_keyCodeMap.find(keyCode);
    if (it != m_keyCodeMap.end())
    {
        return it->second;
    }
    return -1; // KeyCode가 매핑되지 않은 경우 -1 반환
}

void MMMEngine::InputManager::StartUp(HANDLE windowHandle)
{
    m_hWnd = static_cast<HWND>(windowHandle); // 윈도우 핸들
    InitKeyCodeMap();
}

void MMMEngine::InputManager::ShutDown()
{
    m_hWnd = NULL;
    m_keyCodeMap.clear();
}

void MMMEngine::InputManager::Update()
{
    // 마우스 좌표
    ::GetCursorPos(&m_mouseClient);
    ::ScreenToClient(m_hWnd, &m_mouseClient);
    ::GetClientRect(m_hWnd, &m_clientRect);

    // 키보드 상태 업데이트
    memcpy_s(m_prevState, sizeof(m_prevState), m_currState, sizeof(m_currState));
    for (int i = 0; i < 256; i++) {
        m_currState[i] = GetAsyncKeyState(i);
    }


    //// 게임패드 상태 업데이트
    //for (auto& gamepad : m_gamepads)
    //{
    //    if (gamepad)
    //    {
    //        gamepad->Update();
    //    }
    //}
}


Vector2 MMMEngine::InputManager::GetMousePos() 
{
    //(타겟 스크린 사이즈 / 윈도우 클라이언트 사이즈) 비율로 처리하도록 변경
    //float screenFactorX = Screen::GetWidth() / static_cast<float>(RenderManager::Get()->GetWindow()->GetWidth());
    //float screenFactorY = Screen::GetHeight() / static_cast<float>(RenderManager::Get()->GetWindow()->GetHeight());

    return Vector2{ (float)m_mouseClient.x, (float)m_mouseClient.y };
}
bool MMMEngine::InputManager::GetKey(Input::KeyCode keyCode)
{
    auto nativeKeyCode = GetNativeKeyCode(keyCode);
    if (nativeKeyCode == -1) return false;
    return (m_currState[nativeKeyCode] & Input::KEY_PRESSED) != 0;
}
bool MMMEngine::InputManager::GetKeyDown(Input::KeyCode keyCode)
{
    auto nativeKeyCode = GetNativeKeyCode(keyCode);
    if (nativeKeyCode == -1) return false;
    return (!(m_prevState[nativeKeyCode] & Input::KEY_PRESSED) && (m_currState[nativeKeyCode] & Input::KEY_PRESSED));
}
bool MMMEngine::InputManager::GetKeyUp(Input::KeyCode keyCode)
{
    auto nativeKeyCode = GetNativeKeyCode(keyCode);
    if (nativeKeyCode == -1) return false;
    return ((m_prevState[nativeKeyCode] & Input::KEY_PRESSED) && !(m_currState[nativeKeyCode] & Input::KEY_PRESSED));
}
