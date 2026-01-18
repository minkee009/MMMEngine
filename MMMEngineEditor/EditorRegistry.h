#pragma once
#include "GameObject.h"

namespace MMMEngine::EditorRegistry
{
	inline bool g_editor_window_hierarchy = true;
	inline bool g_editor_window_inspector = true;
	inline bool g_editor_window_scenelist = false;
	inline ObjPtr<GameObject> g_selectedGameObject = nullptr;
}

