#pragma once
#include <imgui.h>
#include "Singleton.hpp"

namespace MMMEngine::Editor
{
	class HierarchyWindow : public Utility::Singleton<HierarchyWindow>
	{
	public:
		void Render();
		/// 피킹 등으로 g_selectedGameObject가 자식으로 바뀌었을 때, 다음 Render에서 부모 트리 노드를 열기 위해 호출.
		void RequestExpandParentsForSelection();
	};
}