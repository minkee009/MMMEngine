#include "Canvas.h"
#include "Graphic.h"
#include "RectTransform.h"
#include "RenderManager.h"
#include "UIMask.h"
#include "rttr/registration"
#include <algorithm>
#include <cmath>

RTTR_REGISTRATION
{
	using namespace rttr;
	using namespace MMMEngine;

	registration::enumeration<CanvasScaleMode>("CanvasScaleMode")
			(
				rttr::value("ConstantPixelSize", CanvasScaleMode::ConstantPixelSize),
				rttr::value("ScaleWithScreenSize", CanvasScaleMode::ScaleWithScreenSize)
			);

	registration::class_<Canvas>("Canvas")
		(rttr::metadata("wrapper_type_name", "ObjPtr<Canvas>"))
		.property("SortOrder", &Canvas::GetSortOrder, &Canvas::SetSortOrder)
		.property("ScaleMode", &Canvas::GetScaleMode, &Canvas::SetScaleMode)
		.property("ReferenceResolution", &Canvas::GetReferenceResolution, &Canvas::SetReferenceResolution);

	registration::class_<ObjPtr<Canvas>>("ObjPtr<Canvas>")
		.constructor<>([]() {
			return Object::NewObject<Canvas>();
		})
		.method("Inject", &ObjPtr<Canvas>::Inject);
}

void MMMEngine::Canvas::Initialize()
{
	Behaviour::Initialize();
	RenderManager::Get().RegisterCanvas(this);
}

void MMMEngine::Canvas::UnInitialize()
{
	RenderManager::Get().UnRegisterCanvas(this);
	m_graphics.clear();
	Behaviour::UnInitialize();
}

DirectX::SimpleMath::Vector2 MMMEngine::Canvas::GetCanvasSize()
{
	if (m_scaleMode == CanvasScaleMode::ScaleWithScreenSize)
		return m_referenceResolution;

	UINT w = 0, h = 0;
	RenderManager::Get().GetSceneSize(w, h);
	return { static_cast<float>(w), static_cast<float>(h) };
}

DirectX::SimpleMath::Vector2 MMMEngine::Canvas::GetScaleToScene() const
{
	if (m_scaleMode == CanvasScaleMode::ConstantPixelSize)
		return { 1.0f, 1.0f };

	UINT sceneW = 0, sceneH = 0;
	RenderManager::Get().GetSceneSize(sceneW, sceneH);
	if (m_referenceResolution.x <= 0.0f || m_referenceResolution.y <= 0.0f)
		return { 1.0f, 1.0f };
	const float scaleX = static_cast<float>(sceneW) / m_referenceResolution.x;
	const float scaleY = static_cast<float>(sceneH) / m_referenceResolution.y;
	const float uniform = std::min(scaleX, scaleY);
	return { uniform, uniform };
}

DirectX::SimpleMath::Vector2 MMMEngine::Canvas::GetSceneOffset() const
{
	if (m_scaleMode != CanvasScaleMode::ScaleWithScreenSize)
		return { 0.0f, 0.0f };

	UINT sceneW = 0, sceneH = 0;
	RenderManager::Get().GetSceneSize(sceneW, sceneH);
	if (m_referenceResolution.x <= 0.0f || m_referenceResolution.y <= 0.0f)
		return { 0.0f, 0.0f };

	const float scaleX = static_cast<float>(sceneW) / m_referenceResolution.x;
	const float scaleY = static_cast<float>(sceneH) / m_referenceResolution.y;
	const float uniform = std::min(scaleX, scaleY);
	const DirectX::SimpleMath::Vector2 scaledSize = {
		m_referenceResolution.x * uniform,
		m_referenceResolution.y * uniform
	};
	return {
		(static_cast<float>(sceneW) - scaledSize.x) * 0.5f,
		(static_cast<float>(sceneH) - scaledSize.y) * 0.5f
	};
}

DirectX::SimpleMath::Vector2 MMMEngine::Canvas::ScreenToCanvas(const DirectX::SimpleMath::Vector2& screenPos) const
{
	const auto scale = GetScaleToScene();
	const auto offset = GetSceneOffset();
	const float safeScaleX = (std::abs(scale.x) > 1e-6f) ? scale.x : 1.0f;
	const float safeScaleY = (std::abs(scale.y) > 1e-6f) ? scale.y : 1.0f;
	return {
		(screenPos.x - offset.x) / safeScaleX,
		(screenPos.y - offset.y) / safeScaleY
	};
}

void MMMEngine::Canvas::RegisterGraphic(ObjPtr<Graphic> graphic)
{
	if (!graphic)
		return;

	auto it = std::find(m_graphics.begin(), m_graphics.end(), graphic);
	if (it != m_graphics.end())
		return;

	m_graphics.push_back(graphic);
}

void MMMEngine::Canvas::UnregisterGraphic(ObjPtr<Graphic> graphic)
{
	auto it = std::find(m_graphics.begin(), m_graphics.end(), graphic);
	if (it == m_graphics.end())
		return;

	*it = m_graphics.back();
	m_graphics.pop_back();
}

const std::vector<MMMEngine::ObjPtr<MMMEngine::Graphic>>& MMMEngine::Canvas::GetGraphics() const
{
	return m_graphics;
}

void MMMEngine::Canvas::RenderUI(RenderManager& renderer)
{
	if (!IsActiveAndEnabled())
		return;

	std::vector<ObjPtr<Graphic>> graphics;
	graphics.reserve(m_graphics.size());
	for (auto& graphic : m_graphics)
	{
		if (!graphic.IsValid())
			continue;
		graphics.push_back(graphic);
	}

	std::stable_sort(graphics.begin(), graphics.end(),
		[](const ObjPtr<Graphic>& a, const ObjPtr<Graphic>& b)
		{
			return a->GetRenderOrder() < b->GetRenderOrder();
		});

	if (graphics.empty())
		return;

	renderer.SetUIStencilDisabled();
	renderer.SetUIColorWriteEnabled(true);
	renderer.SetUIMaskParams(false, 0.0f);

	struct MaskEntry
	{
		ObjPtr<UIMask> mask;
		ObjPtr<Graphic> graphic;
		float alphaThreshold = 0.001f;
		bool showGraphic = true;
	};

	auto setStencilForDepth = [&renderer](size_t depth)
	{
		if (depth > 0)
			renderer.SetUIStencilTest(static_cast<UINT>(depth));
		else
			renderer.SetUIStencilDisabled();
	};

	auto drawMaskPass = [&renderer](const MaskEntry& mask, UINT depth, bool increment)
	{
		if (!mask.graphic.IsValid() || !mask.graphic->IsActiveAndEnabled())
			return;

		renderer.SetUIMaskParams(true, mask.alphaThreshold);
		if (increment)
			renderer.SetUIStencilWriteIncrement(depth);
		else
			renderer.SetUIStencilWriteDecrement(depth);

		renderer.SetUIColorWriteEnabled(false);
		mask.graphic->RenderUI(renderer);
		renderer.SetUIColorWriteEnabled(true);
		renderer.SetUIMaskParams(false, 0.0f);
	};

	auto collectMasks = [this](const ObjPtr<Graphic>& graphic, std::vector<MaskEntry>& out)
	{
		out.clear();
		if (!graphic.IsValid())
			return;

		auto tr = graphic->GetTransform();
		if (!tr.IsValid())
			return;

		for (auto t = tr; t != nullptr; t = t->GetParent())
		{
			auto go = t->GetGameObject();
			if (go.IsValid())
			{
				auto mask = go->GetComponent<UIMask>();
				if (mask.IsValid() && mask->IsActiveAndEnabled())
				{
					auto maskGraphic = mask->GetTargetGraphic();
					if (maskGraphic.IsValid()
						&& maskGraphic->IsActiveAndEnabled()
						&& maskGraphic->GetCanvas().operator->() == this)
					{
						MaskEntry entry;
						entry.mask = mask;
						entry.graphic = maskGraphic;
						entry.alphaThreshold = mask->GetAlphaThreshold();
						entry.showGraphic = mask->GetShowGraphic();
						out.push_back(entry);
					}
				}
			}

			if (t == GetTransform())
				break;
		}

		std::reverse(out.begin(), out.end());
	};

	std::vector<MaskEntry> activeMasks;
	activeMasks.reserve(8);
	std::vector<MaskEntry> targetMasks;
	targetMasks.reserve(8);

	for (auto& graphic : graphics)
	{
		if (!graphic->IsActiveAndEnabled())
			continue;

		collectMasks(graphic, targetMasks);
		bool isMaskGraphic = false;
		bool showMaskGraphic = true;
		for (const auto& mask : targetMasks)
		{
			if (mask.graphic == graphic)
			{
				isMaskGraphic = true;
				showMaskGraphic = mask.showGraphic;
				break;
			}
		}

		size_t common = 0;
		const size_t activeCount = activeMasks.size();
		const size_t targetCount = targetMasks.size();
		while (common < activeCount && common < targetCount)
		{
			if (activeMasks[common].mask != targetMasks[common].mask)
				break;
			++common;
		}

		for (size_t i = activeMasks.size(); i-- > common;)
		{
			drawMaskPass(activeMasks[i], static_cast<UINT>(activeMasks.size()), false);
			activeMasks.pop_back();
		}

		for (size_t i = common; i < targetMasks.size(); ++i)
		{
			auto& mask = targetMasks[i];
			drawMaskPass(mask, static_cast<UINT>(activeMasks.size()), true);
			activeMasks.push_back(mask);
		}

		if (!(isMaskGraphic && !showMaskGraphic))
		{
			setStencilForDepth(activeMasks.size());
			graphic->RenderUI(renderer);
		}
	}

	for (size_t i = activeMasks.size(); i-- > 0;)
	{
		drawMaskPass(activeMasks[i], static_cast<UINT>(activeMasks.size()), false);
		activeMasks.pop_back();
	}

	renderer.SetUIStencilDisabled();
	renderer.SetUIColorWriteEnabled(true);
	renderer.SetUIMaskParams(false, 0.0f);
}
