#include "MMMInput.h"
#include "InputManager.h"

Vector2 MMMEngine::Input::GetMousePos()
{
    return InputManager::Get().GetMousePos();
}

bool MMMEngine::Input::GetKey(KeyCode keyCode)
{
    return InputManager::Get().GetKey(keyCode);
}

bool MMMEngine::Input::GetKeyDown(KeyCode keyCode)
{
    return InputManager::Get().GetKeyDown(keyCode);
}

bool MMMEngine::Input::GetKeyUp(KeyCode keyCode)
{
    return InputManager::Get().GetKeyUp(keyCode);
}
