#pragma once
#define NOMINMAX
#include <imgui.h>
#include "Singleton.hpp"
#include "SimpleMath.h"
#include "rttr/type"
#include <unordered_map>
#include <string>
#include "Object.h"

namespace MMMEngine
{
	class Component;
	class GameObject;
}

namespace MMMEngine::Editor
{
	class InspectorWindow : public Utility::Singleton<InspectorWindow>
	{
	private:
		DirectX::SimpleMath::Vector3 m_eulerCache;
		ObjPtr<GameObject> m_lastSelected = nullptr;
		std::vector<ObjPtr<Component>> m_pendingRemoveComponents;
		std::vector<rttr::type> m_componentTypes;
		std::unordered_map<std::string, std::string> m_stringEditCache;

		void RenderProperties(rttr::instance inst);
		void RefreshComponentTypes();
	public:
		void Render();
		void ClearCache();
	};
}
