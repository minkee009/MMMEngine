#pragma once

#include "Export.h"
#include "ExportSingleton.hpp"
#include "SimpleMath.h"

namespace MMMEngine
{
	class MMMENGINE_API UIEventManager : public Utility::ExportSingleton<UIEventManager>
	{
		friend class Utility::ExportSingleton<UIEventManager>;
	private:
		UIEventManager() = default;

	public:
		void UpdateFromScenePointer(const DirectX::SimpleMath::Vector2& scenePos, bool isMouseDown, bool isValid);
		void UpdateFromClientPointer(const DirectX::SimpleMath::Vector2& clientPos, bool isMouseDown);
	};
}

