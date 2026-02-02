#include "UIEventManager.h"

#include "RenderManager.h"
#include "Canvas.h"
#include "Graphic.h"
#include "Button.h"
#include "HandleGage.h"

DEFINE_SINGLETON(MMMEngine::UIEventManager)

namespace
{
	using namespace DirectX::SimpleMath;

	Vector2 ClampToRect(const Vector2& pos, const MMMEngine::RenderManager::SceneViewportRect& rect)
	{
		Vector2 clamped = pos;
		if (clamped.x < rect.x) clamped.x = rect.x;
		if (clamped.y < rect.y) clamped.y = rect.y;
		if (clamped.x > rect.x + rect.width) clamped.x = rect.x + rect.width;
		if (clamped.y > rect.y + rect.height) clamped.y = rect.y + rect.height;
		return clamped;
	}
}

void MMMEngine::UIEventManager::UpdateFromScenePointer(const DirectX::SimpleMath::Vector2& scenePos,
	bool isMouseDown,
	bool isValid)
{
	auto& renderer = RenderManager::Get();
	const auto& canvases = renderer.GetCanvases();
	if (canvases.empty())
		return;

	const Vector2 invalidPos = Vector2(-1000000.0f, -1000000.0f);
	const Vector2 pointerScene = isValid ? scenePos : invalidPos;
	const bool effectiveMouseDown = isMouseDown && isValid;

	for (auto* canvas : canvases)
	{
		if (!canvas || !canvas->IsActiveAndEnabled())
			continue;

		Vector2 pointerCanvas = pointerScene;
		const auto offset = canvas->GetSceneOffset();
		pointerCanvas.x -= offset.x;
		pointerCanvas.y -= offset.y;
		const auto scale = canvas->GetScaleToScene();
		if (scale.x != 0.0f && scale.y != 0.0f)
		{
			pointerCanvas.x /= scale.x;
			pointerCanvas.y /= scale.y;
		}

		const auto canvasSize = canvas->GetCanvasSize();
		const auto& graphics = canvas->GetGraphics();
		for (const auto& graphic : graphics)
		{
			if (!graphic.IsValid() || !graphic->IsActiveAndEnabled())
				continue;

			if (auto button = graphic.Cast<Button>(); button.IsValid())
			{
				button->UpdatePointer(canvasSize, pointerCanvas, effectiveMouseDown);
				continue;
			}

			if (auto gage = graphic.Cast<HandleGage>(); gage.IsValid())
			{
				gage->UpdatePointer(canvasSize, pointerCanvas, effectiveMouseDown);
				continue;
			}
		}
	}
}

void MMMEngine::UIEventManager::UpdateFromClientPointer(const DirectX::SimpleMath::Vector2& clientPos,
	bool isMouseDown)
{
	auto& renderer = RenderManager::Get();
	RenderManager::SceneViewportRect rect{};
	if (!renderer.GetSceneDisplayRect(rect))
		return;

	if (rect.width <= 0.0f || rect.height <= 0.0f)
		return;

	const bool inside = clientPos.x >= rect.x
		&& clientPos.x <= rect.x + rect.width
		&& clientPos.y >= rect.y
		&& clientPos.y <= rect.y + rect.height;

	if (!inside)
	{
		UpdateFromScenePointer({}, false, false);
		return;
	}

	const auto clamped = ClampToRect(clientPos, rect);
	const float sceneW = static_cast<float>(renderer.GetSceneWidth());
	const float sceneH = static_cast<float>(renderer.GetSceneHeight());
	if (sceneW <= 0.0f || sceneH <= 0.0f)
		return;

	const float normX = (clamped.x - rect.x) / rect.width;
	const float normY = (clamped.y - rect.y) / rect.height;
	Vector2 scenePos{ normX * sceneW, normY * sceneH };

	UpdateFromScenePointer(scenePos, isMouseDown, true);
}


