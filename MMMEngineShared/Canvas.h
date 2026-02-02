#pragma once

#include "Export.h"
#include "Behaviour.h"
#include "SimpleMath.h"
#include <vector>

namespace MMMEngine
{
	class Graphic;
	class RenderManager;

	/// Unity Canvas Scaler 참조: Screen Space 전용 스케일링
	enum class CanvasScaleMode
	{
		ConstantPixelSize,  /// 픽셀 크기 고정 (화면 해상도와 무관)
		ScaleWithScreenSize /// 기준 해상도 대비 화면 크기에 따라 스케일
	};

	class MMMENGINE_API Canvas : public Behaviour
	{
	private:
		RTTR_ENABLE(Behaviour)
		RTTR_REGISTRATION_FRIEND

		std::vector<ObjPtr<Graphic>> m_graphics;
		int m_sortOrder = 0;

		CanvasScaleMode m_scaleMode = CanvasScaleMode::ConstantPixelSize;
		DirectX::SimpleMath::Vector2 m_referenceResolution = { 1920.0f, 1080.0f };

	public:
		Canvas() = default;
		virtual ~Canvas() = default;

		void Initialize() override;
		void UnInitialize() override;
		bool RequiresRectTransform() const override { return true; }

		int GetSortOrder() const { return m_sortOrder; }
		void SetSortOrder(int order) { m_sortOrder = order; }

		CanvasScaleMode GetScaleMode() const { return m_scaleMode; }
		void SetScaleMode(CanvasScaleMode mode) { m_scaleMode = mode; }

		const DirectX::SimpleMath::Vector2& GetReferenceResolution() const { return m_referenceResolution; }
		void SetReferenceResolution(const DirectX::SimpleMath::Vector2& res) { m_referenceResolution = res; }

		/// 레이아웃용 캔버스 크기 (SceneSize 또는 ReferenceResolution)
		DirectX::SimpleMath::Vector2 GetCanvasSize();

		/// 캔버스 좌표 -> 씬 픽셀 변환 스케일 (ConstantPixelSize=(1,1), ScaleWithScreenSize=scene/ref, 균일 스케일)
		DirectX::SimpleMath::Vector2 GetScaleToScene() const;
		/// ScaleWithScreenSize일 때 씬 내 여백(레터박스) 오프셋
		DirectX::SimpleMath::Vector2 GetSceneOffset() const;

		void RegisterGraphic(ObjPtr<Graphic> graphic);
		void UnregisterGraphic(ObjPtr<Graphic> graphic);
		const std::vector<ObjPtr<Graphic>>& GetGraphics() const;

		void RenderUI(RenderManager& renderer);
	};
}
