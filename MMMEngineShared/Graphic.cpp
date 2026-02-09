#include "Graphic.h"
#include "Canvas.h"
#include "RectTransform.h"
#include "RenderManager.h"
#include "Texture2D.h"
#include "rttr/registration"
#include <vector>

RTTR_REGISTRATION
{
	using namespace rttr;
	using namespace MMMEngine;

	registration::class_<Graphic>("Graphic")
		(rttr::metadata("wrapper_type_name", "ObjPtr<Graphic>"), rttr::metadata("INSPECTOR","DONT_ADD_COMP"))
		.property("RenderOrder", &Graphic::GetRenderOrder, &Graphic::SetRenderOrder);

	registration::class_<ObjPtr<Graphic>>("ObjPtr<Graphic>")
		.constructor<>([]() {
			return Object::NewObject<Graphic>();
		})
		.method("Inject", &ObjPtr<Graphic>::Inject);
}

void MMMEngine::Graphic::Initialize()
{
	Behaviour::Initialize();

	auto tr = GetTransform();
	if (tr.IsValid())
		tr->onUpdateTransformTree.AddListener<Graphic, &Graphic::HandleTransformParentChanged>(this);

	RefreshCanvas(tr.IsValid() ? tr->GetParent() : ObjPtr<Transform>());
	MarkEffectiveOrderDirtyRecursive();
}

void MMMEngine::Graphic::UnInitialize()
{
	if (GetGameObject().IsValid() && !GetGameObject()->IsDestroyed())
		MarkEffectiveOrderDirtyRecursive();

	auto tr = GetTransform();
	if (tr.IsValid())
		tr->onUpdateTransformTree.RemoveListener<Graphic, &Graphic::HandleTransformParentChanged>(this);

	if (m_canvas.IsValid())
		m_canvas->UnregisterGraphic(SelfPtr(this));
	m_canvas = nullptr;

	Behaviour::UnInitialize();
}

void MMMEngine::Graphic::ApplyNativeSizeFromTexture(const ResPtr<Texture2D>& texture)
{
	if (!texture)
		return;

	const auto rectTransform = GetRectTransform();
	if (!rectTransform.IsValid())
		return;

	const auto width = texture->GetWidth();
	const auto height = texture->GetHeight();
	if (width == 0 || height == 0)
		return;

	rectTransform->SetSizeDelta({ static_cast<float>(width), static_cast<float>(height) });
}

void MMMEngine::Graphic::RefreshCanvas(ObjPtr<Transform> newParent)
{
	ObjPtr<Canvas> foundCanvas = nullptr;

	if (auto selfCanvas = GetGameObject()->GetComponent<Canvas>(); selfCanvas.IsValid())
	{
		foundCanvas = selfCanvas;
	}
	else
	{
		for (auto t = newParent; t != nullptr; t = t->GetParent())
		{
			auto go = t->GetGameObject();
			if (!go.IsValid())
				continue;

			if (auto canvas = go->GetComponent<Canvas>(); canvas.IsValid())
			{
				foundCanvas = canvas;
				break;
			}
		}
	}

	if (foundCanvas == m_canvas)
		return;

	if (m_canvas.IsValid())
		m_canvas->UnregisterGraphic(SelfPtr(this));

	m_canvas = foundCanvas;

	if (m_canvas.IsValid())
		m_canvas->RegisterGraphic(SelfPtr(this));
}

void MMMEngine::Graphic::HandleTransformParentChanged(ObjPtr<Transform> newParent)
{
	MarkEffectiveOrderDirty();
	RefreshCanvas(newParent);
}

MMMEngine::ObjPtr<MMMEngine::RectTransform> MMMEngine::Graphic::GetRectTransform()
{
	auto tr = GetTransform();
	if (!tr.IsValid())
		return nullptr;

	return tr.Cast<RectTransform>();
}

void MMMEngine::Graphic::RefreshCanvasNow()
{
	auto tr = GetTransform();
	RefreshCanvas(tr.IsValid() ? tr->GetParent() : ObjPtr<Transform>());
}

void MMMEngine::Graphic::MarkEffectiveOrderDirty()
{
	m_effectiveOrderDirty = true;
}

void MMMEngine::Graphic::MarkEffectiveOrderDirtyRecursive()
{
	MarkEffectiveOrderDirty();

	auto tr = GetTransform();
	if (!tr.IsValid() || tr->IsDestroyed())
		return;

	if (auto owner = GetGameObject(); owner.IsValid() && !owner->IsDestroyed())
	{
		auto graphics = owner->GetComponents<Graphic>();
		for (auto& graphic : graphics)
		{
			if (graphic.IsValid() && !graphic->IsDestroyed())
				graphic->MarkEffectiveOrderDirty();
		}
	}

	std::vector<ObjPtr<Transform>> stack;
	stack.reserve(tr->GetChildCount());
	const size_t rootChildCount = tr->GetChildCount();
	for (size_t i = 0; i < rootChildCount; ++i)
		stack.push_back(tr->GetChild(i));

	while (!stack.empty())
	{
		auto current = stack.back();
		stack.pop_back();
		if (!current.IsValid() || current->IsDestroyed())
			continue;

		auto go = current->GetGameObject();
		if (go.IsValid() && !go->IsDestroyed())
		{
			auto graphics = go->GetComponents<Graphic>();
			for (auto& graphic : graphics)
			{
				if (graphic.IsValid() && !graphic->IsDestroyed())
					graphic->MarkEffectiveOrderDirty();
			}
		}

		const size_t childCount = current->GetChildCount();
		for (size_t i = 0; i < childCount; ++i)
			stack.push_back(current->GetChild(i));
	}
}

int MMMEngine::Graphic::RecalculateEffectiveRenderOrder()
{
	int order = m_renderOrder;
	m_cachedRenderOrderValue = m_renderOrder;
	m_cachedParentGraphic = nullptr;
	m_cachedParentEffectiveOrder = 0;
	m_hadCachedParentGraphic = false;

	auto tr = GetTransform();
	for (auto parent = tr.IsValid() ? tr->GetParent() : ObjPtr<Transform>();
		parent.IsValid();
		parent = parent->GetParent())
	{
		if (parent->IsDestroyed())
			continue;

		auto go = parent->GetGameObject();
		if (!go.IsValid() || go->IsDestroyed())
			continue;

		auto parentGraphic = go->GetComponent<Graphic>();
		if (parentGraphic.IsValid() && !parentGraphic->IsDestroyed())
		{
			m_cachedParentGraphic = parentGraphic;
			m_cachedParentEffectiveOrder = parentGraphic->GetEffectiveRenderOrder();
			m_hadCachedParentGraphic = true;
			order += m_cachedParentEffectiveOrder;
			break;
		}
	}

	return order;
}

int MMMEngine::Graphic::GetEffectiveRenderOrder()
{
	int currentParentEffective = 0;
	bool parentChanged = false;
	if (m_cachedParentGraphic.IsValid())
	{
		currentParentEffective = m_cachedParentGraphic->GetEffectiveRenderOrder();
		parentChanged = (currentParentEffective != m_cachedParentEffectiveOrder);
	}
	else if (m_hadCachedParentGraphic)
	{
		parentChanged = true;
	}

	if (m_effectiveOrderDirty || m_cachedRenderOrderValue != m_renderOrder || parentChanged)
	{
		m_cachedEffectiveRenderOrder = RecalculateEffectiveRenderOrder();
		m_effectiveOrderDirty = false;
	}

	return m_cachedEffectiveRenderOrder;
}

void MMMEngine::Graphic::RenderUI(RenderManager& renderer)
{
	if (!m_canvas.IsValid())
		return;

	auto rectTransform = GetRectTransform();
	if (!rectTransform.IsValid())
		return;

	auto canvasSize = m_canvas->GetCanvasSize();
	auto rect = rectTransform->GetRectInCanvas(canvasSize);

	// 캔버스 좌표를 씬 픽셀로 변환 (ConstantPixelSize=1:1, ScaleWithScreenSize=ref기준 스케일)
	auto scale = m_canvas->GetScaleToScene();
	rect.x *= scale.x;
	rect.y *= scale.y;
	rect.z *= scale.x;
	rect.w *= scale.y;
	const auto offset = m_canvas->GetSceneOffset();
	rect.x += offset.x;
	rect.y += offset.y;

	const auto pivot = rectTransform->GetPivot();
	const DirectX::SimpleMath::Vector2 pivotScene = {
		rect.x + rect.z * pivot.x,
		rect.y + rect.w * pivot.y
	};
	const DirectX::SimpleMath::Vector2 pivotN = {
		rect.z != 0.0f ? (pivotScene.x - rect.x) / rect.z : 0.0f,
		rect.w != 0.0f ? (pivotScene.y - rect.y) / rect.w : 0.0f
	};
	const auto worldMat = rectTransform->GetWorldMatrix();
	DirectX::SimpleMath::Vector2 rightDir = { worldMat._11, worldMat._12 };
	DirectX::SimpleMath::Vector2 upDir = { worldMat._21, worldMat._22 };
	const float rightLen = std::sqrt(rightDir.x * rightDir.x + rightDir.y * rightDir.y);
	const float upLen = std::sqrt(upDir.x * upDir.x + upDir.y * upDir.y);
	if (rightLen > 1e-6f) rightDir /= rightLen; else rightDir = { 1.0f, 0.0f };
	if (upLen > 1e-6f) upDir /= upLen; else upDir = { 0.0f, 1.0f };

	renderer.DrawUIElement(rect, GetUVRect(), m_color, m_texture, pivotN, rightDir, upDir);
}
