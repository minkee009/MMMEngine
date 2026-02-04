#include "imgui.h"
#include "SceneViewWindow.h"
#include "EditorRegistry.h"
#include "HierarchyWindow.h"
#include "RenderStateGuard.h"
#include "RenderManager.h"
#include "ResourceManager.h"
#include "Renderer.h"
#include "VShader.h"
#include "PShader.h"
#include "SceneManager.h"
#include <ImGuizmo.h>
#include "Transform.h"
#include <imgui_internal.h>
#include "ColliderComponent.h"
#include "Camera.h"
#include "RigidBodyComponent.h"
#include "Canvas.h"
#include "Graphic.h"
#include "RectTransform.h"
#include "MMMTime.h"
#include <memory>
#include <algorithm>
#include <cmath>
#include <vector>
#include <physx/PxPhysicsAPI.h>

using namespace MMMEngine::Editor;
using namespace MMMEngine;
using namespace MMMEngine::Utility;
using namespace MMMEngine::EditorRegistry;

namespace
{
	struct PickingIdBuffer
	{
		uint32_t objectId = 0;
		uint32_t padding[3] = { 0, 0, 0 };
	};

	struct OutlineConstants
	{
		DirectX::SimpleMath::Vector4 color = { 1.0f, 0.5f, 0.0f, 1.0f };
		DirectX::SimpleMath::Vector2 texelSize = { 1.0f, 1.0f };
		DirectX::SimpleMath::Vector2 padding0 = { 0.0f, 0.0f };
		float thickness = 1.0f;
		float threshold = 0.2f;
		DirectX::SimpleMath::Vector2 padding1 = { 0.0f, 0.0f };
	};

	struct UiCanvasInfo
	{
		DirectX::SimpleMath::Vector2 canvasSize = { 0.0f, 0.0f };
		DirectX::SimpleMath::Vector2 scaleToScene = { 1.0f, 1.0f };
		DirectX::SimpleMath::Vector2 sceneOffset = { 0.0f, 0.0f };
	};

	UiCanvasInfo GetCanvasInfo(Canvas* canvas, float sceneWidth, float sceneHeight)
	{
		using namespace DirectX::SimpleMath;
		UiCanvasInfo info;
		Vector2 sceneSize = { sceneWidth, sceneHeight };
		if (!canvas)
		{
			info.canvasSize = sceneSize;
			info.scaleToScene = { 1.0f, 1.0f };
			info.sceneOffset = { 0.0f, 0.0f };
			return info;
		}

		if (canvas->GetScaleMode() == CanvasScaleMode::ScaleWithScreenSize)
		{
			auto ref = canvas->GetReferenceResolution();
			info.canvasSize = ref;
			const float scaleX = ref.x > 0.0f ? sceneWidth / ref.x : 1.0f;
			const float scaleY = ref.y > 0.0f ? sceneHeight / ref.y : 1.0f;
			const float uniform = std::min(scaleX, scaleY);
			info.scaleToScene = { uniform, uniform };
			const DirectX::SimpleMath::Vector2 scaledSize = {
				ref.x * uniform,
				ref.y * uniform
			};
			info.sceneOffset = {
				(sceneWidth - scaledSize.x) * 0.5f,
				(sceneHeight - scaledSize.y) * 0.5f
			};
			return info;
		}

		info.canvasSize = sceneSize;
		info.scaleToScene = { 1.0f, 1.0f };
		info.sceneOffset = { 0.0f, 0.0f };
		return info;
	}

	Canvas* FindCanvasForTransform(const ObjPtr<Transform>& tr)
	{
		for (auto t = tr; t != nullptr; t = t->GetParent())
		{
			auto go = t->GetGameObject();
			if (!go.IsValid())
				continue;
			if (auto canvas = go->GetComponent<Canvas>(); canvas.IsValid())
				return canvas.operator->();
		}
		return nullptr;
	}

	void ComputeAnchorData(ObjPtr<RectTransform> rect,
		const DirectX::SimpleMath::Vector2& canvasSize,
		DirectX::SimpleMath::Vector2& anchorCenter,
		DirectX::SimpleMath::Vector2& anchorSpan)
	{
		if (!rect.IsValid())
		{
			anchorCenter = DirectX::SimpleMath::Vector2::Zero;
			anchorSpan = DirectX::SimpleMath::Vector2::Zero;
			return;
		}

		rect->GetAnchorData(canvasSize, anchorCenter, anchorSpan);
	}

	void ComputeParentBasis(const ObjPtr<RectTransform>& rect,
		DirectX::SimpleMath::Vector2& parentRight,
		DirectX::SimpleMath::Vector2& parentUp)
	{
		using namespace DirectX::SimpleMath;
		parentRight = Vector2::UnitX;
		parentUp = Vector2::UnitY;

		if (!rect.IsValid())
			return;

		if (auto parent = rect->GetParent())
		{
			if (auto parentRect = parent.Cast<RectTransform>())
			{
				const auto worldMat = parentRect->GetWorldMatrix();
				parentRight = { worldMat._11, worldMat._12 };
				parentUp = { worldMat._21, worldMat._22 };
				const float rightLen = std::sqrt(parentRight.x * parentRight.x + parentRight.y * parentRight.y);
				const float upLen = std::sqrt(parentUp.x * parentUp.x + parentUp.y * parentUp.y);
				if (rightLen > 1e-6f) parentRight /= rightLen; else parentRight = Vector2::UnitX;
				if (upLen > 1e-6f) parentUp /= upLen; else parentUp = Vector2::UnitY;
			}
		}
	}

	bool PointInRect(float px, float py, float rx, float ry, float rw, float rh)
	{
		return px >= rx && px <= rx + rw && py >= ry && py <= ry + rh;
	}

	bool PointInRotatedRect(float px, float py,
		const DirectX::SimpleMath::Vector4& rectScene,
		const DirectX::SimpleMath::Vector2& pivot,
		const DirectX::SimpleMath::Quaternion& worldRot)
	{
		using namespace DirectX::SimpleMath;
		if (rectScene.z <= 1e-6f || rectScene.w <= 1e-6f)
			return false;

		const Vector2 pivotPosScene = {
			rectScene.x + rectScene.z * pivot.x,
			rectScene.y + rectScene.w * pivot.y
		};

		const auto right3 = Vector3::Transform(Vector3::UnitX, worldRot);
		const auto up3 = Vector3::Transform(Vector3::UnitY, worldRot);
		Vector2 rightDir = { right3.x, right3.y };
		Vector2 upDir = { up3.x, up3.y };
		const float rightLen = std::sqrt(rightDir.x * rightDir.x + rightDir.y * rightDir.y);
		const float upLen = std::sqrt(upDir.x * upDir.x + upDir.y * upDir.y);
		if (rightLen > 1e-6f) rightDir /= rightLen; else rightDir = { 1.0f, 0.0f };
		if (upLen > 1e-6f) upDir /= upLen; else upDir = { 0.0f, 1.0f };

		const Vector2 d = { px - pivotPosScene.x, py - pivotPosScene.y };
		const float localX = d.x * rightDir.x + d.y * rightDir.y;
		const float localY = d.x * upDir.x + d.y * upDir.y;

		const float localU = localX / rectScene.z + pivot.x;
		const float localV = localY / rectScene.w + pivot.y;
		return localU >= 0.0f && localU <= 1.0f && localV >= 0.0f && localV <= 1.0f;
	}

	float SmoothStep01(float t)
	{
		t = std::clamp(t, 0.0f, 1.0f);
		return t * t * (3.0f - 2.0f * t);
	}
}

void MMMEngine::Editor::SceneViewWindow::Initialize(ID3D11Device* device, ID3D11DeviceContext* context, int initWidth, int initHeight)
{
	m_cachedDevice = device;
	m_cachedContext = context;

	m_width = initWidth;
	m_height = initHeight;
	m_lastWidth = initWidth;
	m_lastHeight = initHeight;

	if (!m_pCam)
	{
		m_pCam = std::make_unique<EditorCamera>();
		m_pCam->SetPosition(0.0f, 5.0f, 10.0f);
		m_pCam->SetEulerRotation(DirectX::SimpleMath::Vector3(15.0f, 180.0f, 0.0f));
		m_pCam->SetFOV(60.0f);
		m_pCam->SetNearPlane(0.1f);
		m_pCam->SetFarPlane(1000.0f);
		m_pCam->SetAspectRatio((float)initWidth, (float)initHeight);
		m_viewGizmoDistance = 10.0f;
	}

	m_pGridRenderer = std::make_unique<EditorGridRenderer>();
	if (!m_pGridRenderer->Initialize(device))
	{
		// 에러 처리
		OutputDebugStringA("Failed to initialize Grid Renderer\n");
	}

	CreateRenderTargets(device, m_lastWidth, m_lastHeight);

	m_states = std::make_unique<CommonStates>(m_cachedDevice);
	m_batch = std::make_unique<PrimitiveBatch<VertexPositionColor>>(m_cachedContext);
	m_effect = std::make_unique<BasicEffect>(m_cachedDevice);
	m_effect->SetVertexColorEnabled(true);
	{
		void const* shaderByteCode;
		size_t byteCodeLength;

		m_effect->GetVertexShaderBytecode(&shaderByteCode, &byteCodeLength);

		m_cachedDevice->CreateInputLayout(
			VertexPositionColor::InputElements, VertexPositionColor::InputElementCount,
			shaderByteCode, byteCodeLength,
			m_pDebugDrawIL.ReleaseAndGetAddressOf());
	}

	// Picking ID constant buffer
	{
		D3D11_BUFFER_DESC bd = {};
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		bd.ByteWidth = sizeof(PickingIdBuffer);
		m_cachedDevice->CreateBuffer(&bd, nullptr, m_pPickingIdBuffer.GetAddressOf());
	}

	// Outline constant buffer
	{
		D3D11_BUFFER_DESC bd = {};
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		bd.ByteWidth = sizeof(OutlineConstants);
		m_cachedDevice->CreateBuffer(&bd, nullptr, m_pOutlineCBuffer.GetAddressOf());
	}

	// Mask depth state (always pass, no write)
	{
		D3D11_DEPTH_STENCIL_DESC dsDesc = {};
		dsDesc.DepthEnable = TRUE;
		dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		dsDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;
		dsDesc.StencilEnable = FALSE;
		m_cachedDevice->CreateDepthStencilState(&dsDesc, m_pMaskDepthState.GetAddressOf());
	}

	// No-color-write blend state (stencil-only pass)
	{
		D3D11_BLEND_DESC blendDesc = {};
		blendDesc.RenderTarget[0].BlendEnable = FALSE;
		blendDesc.RenderTarget[0].RenderTargetWriteMask = 0;
		m_cachedDevice->CreateBlendState(&blendDesc, m_pNoColorWriteBS.GetAddressOf());
	}

	// Stencil write state (always pass, replace)
	{
		D3D11_DEPTH_STENCIL_DESC dsDesc = {};
		dsDesc.DepthEnable = TRUE;
		dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		dsDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;
		dsDesc.StencilEnable = TRUE;
		dsDesc.StencilReadMask = 0xFF;
		dsDesc.StencilWriteMask = 0xFF;
		dsDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
		dsDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
		dsDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_REPLACE;
		dsDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
		dsDesc.BackFace = dsDesc.FrontFace;
		m_cachedDevice->CreateDepthStencilState(&dsDesc, m_pStencilWriteState.GetAddressOf());
	}

	// Stencil test state (equal)
	{
		D3D11_DEPTH_STENCIL_DESC dsDesc = {};
		dsDesc.DepthEnable = FALSE;
		dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		dsDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;
		dsDesc.StencilEnable = TRUE;
		dsDesc.StencilReadMask = 0xFF;
		dsDesc.StencilWriteMask = 0xFF;
		dsDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
		dsDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
		dsDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
		dsDesc.FrontFace.StencilFunc = D3D11_COMPARISON_EQUAL;
		dsDesc.BackFace = dsDesc.FrontFace;
		m_cachedDevice->CreateDepthStencilState(&dsDesc, m_pStencilTestState.GetAddressOf());
	}

	// Outline blend state
	{
		D3D11_BLEND_DESC blendDesc = {};
		blendDesc.RenderTarget[0].BlendEnable = TRUE;
		blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
		blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
		blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		// Preserve destination alpha so ImGui doesn't treat the scene as transparent.
		blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ZERO;
		blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
		blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		m_cachedDevice->CreateBlendState(&blendDesc, m_pOutlineBlendState.GetAddressOf());
	}
}

void MMMEngine::Editor::SceneViewWindow::Render()
{
	if (ImGui::IsKeyPressed(ImGuiKey_F))
	{
		if (g_selectedGameObject.IsValid())
		{
			auto& tr = g_selectedGameObject->GetTransform();
			// 오브젝트의 위치로 포커스 (거리는 5.0f로 설정하거나 바운딩 박스 크기에 비례하게 설정)
			const float focusDistance = 7.0f;
			m_pCam->FocusOn(tr->GetWorldPosition(), focusDistance);
			m_viewGizmoDistance = focusDistance;
			m_viewGizmoPivot = tr->GetWorldPosition();
			m_hasViewGizmoPivot = true;
		}
	}

	if (g_selectedGameObject.IsValid())
	{
		auto tr = g_selectedGameObject->GetTransform();
		if (tr.IsValid() && !tr->IsDestroyed())
		{
			m_viewGizmoPivot = tr->GetWorldPosition();
			m_hasViewGizmoPivot = true;
		}
	}

	if (!m_hasViewGizmoPivot && m_pCam)
	{
		Matrix camWorld = m_pCam->GetTransformMatrix();
		Vector3 forward = camWorld.Forward();
		const float initDistance = std::max(0.1f, m_viewGizmoDistance);
		m_viewGizmoPivot = m_pCam->GetPosition() + forward * initDistance;
		m_hasViewGizmoPivot = true;
	}

	if (m_hasViewGizmoPivot && m_pCam)
	{
		float dist = (m_pCam->GetPosition() - m_viewGizmoPivot).Length();
		if (dist < 0.1f)
			dist = 0.1f;
		m_viewGizmoDistance = dist;
	}

	if (m_viewGizmoTransition.active && m_pCam)
	{
		m_viewGizmoTransition.elapsed += Time::GetUnscaledDeltaTime();
		float t = m_viewGizmoTransition.duration <= 1e-5f
			? 1.0f
			: (m_viewGizmoTransition.elapsed / m_viewGizmoTransition.duration);
		t = SmoothStep01(t);
		m_pCam->SetPosition(Vector3::Lerp(m_viewGizmoTransition.startPos, m_viewGizmoTransition.targetPos, t));
		m_pCam->SetRotation(Quaternion::Slerp(m_viewGizmoTransition.startRot, m_viewGizmoTransition.targetRot, t));
		m_pCam->SyncInputState();

		if (m_viewGizmoTransition.elapsed >= m_viewGizmoTransition.duration)
		{
			m_pCam->SetPosition(m_viewGizmoTransition.targetPos);
			m_pCam->SetRotation(m_viewGizmoTransition.targetRot);
			m_pCam->SyncInputState();
			m_viewGizmoTransition.active = false;
		}
	}

	if (!g_editor_window_sceneView)
		return;

	m_blockCameraInput = m_viewGizmoTransition.active;

	ResizeRenderTarget(m_cachedDevice, m_lastWidth, m_lastHeight);
	RenderSceneToTexture(m_cachedContext);

	ImGuiWindowClass wc;
	wc.ParentViewportId = ImGui::GetMainViewport()->ID;
	wc.ViewportFlagsOverrideSet = ImGuiViewportFlags_NoFocusOnAppearing;
	ImGui::SetNextWindowClass(&wc);

	ImGuiStyle& style = ImGui::GetStyle();
	style.WindowMenuButtonPosition = ImGuiDir_None;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
	ImGui::SetNextWindowSize(ImVec2(static_cast<float>(m_width), static_cast<float>(m_height)), ImGuiCond_FirstUseEver);

	ImGui::Begin(u8"\uf009 씬", &g_editor_window_sceneView);

	if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
	{
		ImGui::SetWindowFocus();
	}

	m_isHovered = ImGui::IsWindowHovered();
	m_isFocused = ImGui::IsWindowFocused();

	if (m_isFocused && m_isHovered)
	{
		ImGuiIO& io = ImGui::GetIO();
		const bool rightDown = ImGui::IsMouseDown(ImGuiMouseButton_Right);

		// 수정 제안: WantCaptureKeyboard 조건을 제거하거나 
		// ImGui Key 관련 함수를 직접 사용하여 우선순위를 높입니다.
		const bool gizmoUsing = ImGuizmo::IsUsing();

		if (!rightDown && !gizmoUsing)
		{
			if (ImGui::IsKeyPressed(ImGuiKey_Q))
				m_guizmoOperation = (ImGuizmo::OPERATION)0; // 0은 어떤 기즈모도 표시하지 않음

			if (ImGui::IsKeyPressed(ImGuiKey_W))
				m_guizmoOperation = ImGuizmo::TRANSLATE;

			if (ImGui::IsKeyPressed(ImGuiKey_E))
				m_guizmoOperation = ImGuizmo::ROTATE;

			if (ImGui::IsKeyPressed(ImGuiKey_R))
				m_guizmoOperation = ImGuizmo::SCALE;
		}
	}


	// 사용 가능한 영역 크기 가져오기
	ImVec2 viewportSize = ImGui::GetContentRegionAvail();
	m_lastWidth = static_cast<int>(viewportSize.x);
	m_lastHeight = static_cast<int>(viewportSize.y);

	auto scenecornerpos = ImGui::GetCursorPos();

	// ImGui에 텍스처 렌더링
	if (m_pSceneSRV)
	{
		ImGui::Image(
			(ImTextureID)m_pSceneSRV.Get(),
			viewportSize,
			ImVec2(0, 0),
			ImVec2(1, 1)
		);
	}

	ImVec2 imagePos = ImGui::GetItemRectMin();
	ImVec2 imageMax = ImGui::GetItemRectMax();
	ImVec2 imageSize = ImVec2(imageMax.x - imagePos.x, imageMax.y - imagePos.y);
	bool gizmoDrawn = false;
	bool uiResizeHovered = false;
	bool uiResizeUsing = false;
	static bool s_uiDragActive = false;
	static ObjPtr<RectTransform> s_uiDragTarget;
	static DirectX::SimpleMath::Vector2 s_uiDragStartMouseScene;
	static DirectX::SimpleMath::Vector2 s_uiDragStartPivotScene;
	static DirectX::SimpleMath::Vector2 s_uiDragStartAnchorCenter;
	static DirectX::SimpleMath::Vector2 s_uiDragParentRight;
	static DirectX::SimpleMath::Vector2 s_uiDragParentUp;
	static DirectX::SimpleMath::Vector2 s_uiDragScaleToScene;
	static DirectX::SimpleMath::Vector2 s_uiDragSceneOffset;
	static bool s_uiResizeActive = false;
	static int s_uiResizeHandle = -1;
	static ObjPtr<RectTransform> s_uiResizeTarget;
	static DirectX::SimpleMath::Vector2 s_uiResizeStartSizeScene;
	static DirectX::SimpleMath::Vector2 s_uiResizeStartPivotScene;
	static DirectX::SimpleMath::Vector2 s_uiResizeStartAnchorCenter;
	static DirectX::SimpleMath::Vector2 s_uiResizeStartAnchorSpan;
	static DirectX::SimpleMath::Vector2 s_uiResizeParentRight;
	static DirectX::SimpleMath::Vector2 s_uiResizeParentUp;
	static DirectX::SimpleMath::Vector2 s_uiResizeRightDir;
	static DirectX::SimpleMath::Vector2 s_uiResizeUpDir;
	static DirectX::SimpleMath::Vector2 s_uiResizeFixedCornerScene;
	static DirectX::SimpleMath::Vector2 s_uiResizePivot;
	static DirectX::SimpleMath::Vector2 s_uiResizeScaleToScene;
	static DirectX::SimpleMath::Vector2 s_uiResizeSceneOffset;

	if (!m_ui2DMode || !g_selectedGameObject.IsValid())
	{
		s_uiResizeActive = false;
		s_uiDragActive = false;
	}
	// ImGuizmo는 별도의 DrawList에 그려짐
	if (g_selectedGameObject.IsValid())
	{
		if (m_ui2DMode)
		{
			auto rectTr = g_selectedGameObject->GetTransform().Cast<RectTransform>();
			if (s_uiResizeActive && (!rectTr.IsValid() || s_uiResizeTarget != rectTr))
				s_uiResizeActive = false;
			if (s_uiDragActive && (!rectTr.IsValid() || s_uiDragTarget != rectTr))
				s_uiDragActive = false;
			if (rectTr.IsValid())
			{
				ImGuizmo::OPERATION op = m_guizmoOperation;
				if (op != ImGuizmo::TRANSLATE && op != ImGuizmo::SCALE && op != ImGuizmo::ROTATE)
					op = ImGuizmo::TRANSLATE;

				float snapValue[3] = { 0.f, 0.f, 0.f };
				bool useSnap = false;
				const bool hasUiGizmo = (m_guizmoOperation != (ImGuizmo::OPERATION)0);
				if (hasUiGizmo)
				{
					gizmoDrawn = true;
					ImGuizmo::SetRect(imagePos.x, imagePos.y, imageSize.x, imageSize.y);
					useSnap = ImGui::GetIO().KeyCtrl;
					if (useSnap)
					{
						if (op == ImGuizmo::TRANSLATE)
							snapValue[0] = snapValue[1] = snapValue[2] = 1.0f;
						else if (op == ImGuizmo::SCALE)
							snapValue[0] = snapValue[1] = snapValue[2] = 1.0f;
					}
					ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
					ImGuizmo::SetOrthographic(true);
				}

				auto viewMat = Matrix::Identity;
				auto projMat = Matrix::Identity;

				auto canvas = FindCanvasForTransform(rectTr);
				const float sceneWidth = static_cast<float>(m_width);
				const float sceneHeight = static_cast<float>(m_height);
				const auto canvasInfo = GetCanvasInfo(canvas, sceneWidth, sceneHeight);
				auto rectCanvas = rectTr->GetRectInCanvas(canvasInfo.canvasSize);
				auto rectScene = DirectX::SimpleMath::Vector4(
					canvasInfo.sceneOffset.x + rectCanvas.x * canvasInfo.scaleToScene.x,
					canvasInfo.sceneOffset.y + rectCanvas.y * canvasInfo.scaleToScene.y,
					rectCanvas.z * canvasInfo.scaleToScene.x,
					rectCanvas.w * canvasInfo.scaleToScene.y);

				const auto pivot = rectTr->GetPivot();
				const DirectX::SimpleMath::Vector2 pivotPosScene = {
					rectScene.x + rectScene.z * pivot.x,
					rectScene.y + rectScene.w * pivot.y
				};
				const auto worldRot = rectTr->GetWorldRotation();
				const auto right3 = DirectX::SimpleMath::Vector3::Transform(DirectX::SimpleMath::Vector3::UnitX, worldRot);
				const auto up3 = DirectX::SimpleMath::Vector3::Transform(DirectX::SimpleMath::Vector3::UnitY, worldRot);
				DirectX::SimpleMath::Vector2 rightDir = { right3.x, right3.y };
				DirectX::SimpleMath::Vector2 upDir = { up3.x, up3.y };
				const float rightLen = std::sqrt(rightDir.x * rightDir.x + rightDir.y * rightDir.y);
				const float upLen = std::sqrt(upDir.x * upDir.x + upDir.y * upDir.y);
				if (rightLen > 1e-6f) rightDir /= rightLen; else rightDir = { 1.0f, 0.0f };
				if (upLen > 1e-6f) upDir /= upLen; else upDir = { 0.0f, 1.0f };

				auto toScreen = [&](float localX, float localY)
				{
					const DirectX::SimpleMath::Vector2 offset = { localX - rectScene.z * pivot.x, localY - rectScene.w * pivot.y };
					const DirectX::SimpleMath::Vector2 posScene = {
						pivotPosScene.x + rightDir.x * offset.x + upDir.x * offset.y,
						pivotPosScene.y + rightDir.y * offset.x + upDir.y * offset.y
					};
					return ImVec2(imagePos.x + posScene.x, imagePos.y + posScene.y);
				};

				auto toScene = [&](float localX, float localY)
				{
					const DirectX::SimpleMath::Vector2 offset = { localX - rectScene.z * pivot.x, localY - rectScene.w * pivot.y };
					return DirectX::SimpleMath::Vector2{
						pivotPosScene.x + rightDir.x * offset.x + upDir.x * offset.y,
						pivotPosScene.y + rightDir.y * offset.x + upDir.y * offset.y
					};
				};

				const DirectX::SimpleMath::Vector2 p0Scene = toScene(0.0f, 0.0f);
				const DirectX::SimpleMath::Vector2 p1Scene = toScene(rectScene.z, 0.0f);
				const DirectX::SimpleMath::Vector2 p2Scene = toScene(rectScene.z, rectScene.w);
				const DirectX::SimpleMath::Vector2 p3Scene = toScene(0.0f, rectScene.w);

				const ImVec2 p0 = toScreen(0.0f, 0.0f);
				const ImVec2 p1 = toScreen(rectScene.z, 0.0f);
				const ImVec2 p2 = toScreen(rectScene.z, rectScene.w);
				const ImVec2 p3 = toScreen(0.0f, rectScene.w);
				ImDrawList* drawList = ImGui::GetWindowDrawList();

				// 선택된 UI의 RectTransform 영역 표시
				{
					drawList->PushClipRect(imagePos, imageMax, true);
					const ImU32 rectColor = IM_COL32(255, 200, 80, 255);
					drawList->AddQuad(p0, p1, p2, p3, rectColor, 2.0f);

					// 앵커 영역 표시 (부모 영역 기준)
					DirectX::SimpleMath::Vector2 anchorCenter;
					DirectX::SimpleMath::Vector2 anchorSpan;
					ComputeAnchorData(rectTr, canvasInfo.canvasSize, anchorCenter, anchorSpan);
					const DirectX::SimpleMath::Vector2 anchorSceneCenter = {
						canvasInfo.sceneOffset.x + anchorCenter.x * canvasInfo.scaleToScene.x,
						canvasInfo.sceneOffset.y + anchorCenter.y * canvasInfo.scaleToScene.y
					};
					const DirectX::SimpleMath::Vector2 anchorSceneSpan = {
						anchorSpan.x * canvasInfo.scaleToScene.x,
						anchorSpan.y * canvasInfo.scaleToScene.y
					};
					const ImVec2 anchorMin = ImVec2(
						imagePos.x + anchorSceneCenter.x - anchorSceneSpan.x * 0.5f,
						imagePos.y + anchorSceneCenter.y - anchorSceneSpan.y * 0.5f);
					const ImVec2 anchorMax = ImVec2(
						anchorMin.x + anchorSceneSpan.x,
						anchorMin.y + anchorSceneSpan.y);
					const ImU32 anchorColor = IM_COL32(120, 200, 255, 255);
					const char* anchorIconTL = u8"\uf0d8"; // caret-up
					const char* anchorIconTR = u8"\uf0da"; // caret-right
					const char* anchorIconBL = u8"\uf0d9"; // caret-left
					const char* anchorIconBR = u8"\uf0d7"; // caret-down
					auto drawAnchorIcon = [&](ImVec2 pos, const char* icon)
					{
						const ImVec2 iconSize = ImGui::CalcTextSize(icon);
						ImVec2 drawPos = ImVec2(pos.x - iconSize.x * 0.5f, pos.y - iconSize.y * 0.5f);
						drawList->AddText(drawPos, anchorColor, icon);
					};
					drawAnchorIcon(anchorMin, anchorIconTL);
					drawAnchorIcon(ImVec2(anchorMax.x, anchorMin.y), anchorIconTR);
					drawAnchorIcon(ImVec2(anchorMin.x, anchorMax.y), anchorIconBL);
					drawAnchorIcon(anchorMax, anchorIconBR);
					drawList->AddRect(anchorMin, anchorMax, anchorColor, 0.0f, 0, 2.0f);
					drawList->PopClipRect();

					// 피벗 표시
					const ImVec2 pivotPos = ImVec2(
						imagePos.x + rectScene.x + rectScene.z * pivot.x,
						imagePos.y + rectScene.y + rectScene.w * pivot.y);
					const float pivotRadius = 8.0f;
					const ImU32 pivotColor = IM_COL32(120, 200, 255, 255);
					ImDrawList* fg = ImGui::GetForegroundDrawList(ImGui::GetWindowViewport());
					fg->PushClipRect(imagePos, imageMax, true);
					fg->AddCircle(pivotPos, pivotRadius, pivotColor, 0, 3.5f);
					fg->PopClipRect();
				}

				const bool isCanvas = g_selectedGameObject->GetComponent<Canvas>().IsValid();
				const bool showUiHandles = !isCanvas && (m_guizmoOperation == (ImGuizmo::OPERATION)0);
				if (showUiHandles)
				{
					const float handleRadius = 5.0f;
					const ImU32 handleColor = IM_COL32(120, 200, 255, 255);
					const ImU32 handleColorHot = IM_COL32(170, 230, 255, 255);
					const ImVec2 handleScreen[4] = { p0, p1, p2, p3 };
					const DirectX::SimpleMath::Vector2 handleScene[4] = { p0Scene, p1Scene, p2Scene, p3Scene };

					ImVec2 mousePos = ImGui::GetMousePos();
					int hoveredHandle = -1;
					for (int i = 0; i < 4; ++i)
					{
						const float dx = mousePos.x - handleScreen[i].x;
						const float dy = mousePos.y - handleScreen[i].y;
						if (dx * dx + dy * dy <= handleRadius * handleRadius)
						{
							hoveredHandle = i;
							break;
						}
					}

					if (hoveredHandle != -1)
						uiResizeHovered = true;

					if (!s_uiResizeActive && hoveredHandle != -1 && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
					{
						s_uiResizeActive = true;
						s_uiResizeHandle = hoveredHandle;
						s_uiResizeTarget = rectTr;
						s_uiResizeStartSizeScene = { rectScene.z, rectScene.w };
						s_uiResizeStartPivotScene = pivotPosScene;
						s_uiResizePivot = { pivot.x, pivot.y };
						s_uiResizeRightDir = rightDir;
						s_uiResizeUpDir = upDir;

						DirectX::SimpleMath::Vector2 anchorCenter;
						DirectX::SimpleMath::Vector2 anchorSpan;
						ComputeAnchorData(rectTr, canvasInfo.canvasSize, anchorCenter, anchorSpan);
						s_uiResizeStartAnchorCenter = anchorCenter;
						s_uiResizeStartAnchorSpan = anchorSpan;
						ComputeParentBasis(rectTr, s_uiResizeParentRight, s_uiResizeParentUp);
						s_uiResizeScaleToScene = canvasInfo.scaleToScene;
						s_uiResizeSceneOffset = canvasInfo.sceneOffset;

						switch (hoveredHandle)
						{
						case 0: s_uiResizeFixedCornerScene = handleScene[2]; break; // TL -> BR
						case 1: s_uiResizeFixedCornerScene = handleScene[3]; break; // TR -> BL
						case 2: s_uiResizeFixedCornerScene = handleScene[0]; break; // BR -> TL
						case 3: s_uiResizeFixedCornerScene = handleScene[1]; break; // BL -> TR
						}
					}

					if (s_uiResizeActive && s_uiResizeTarget == rectTr)
					{
						uiResizeUsing = true;
						if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
						{
							s_uiResizeActive = false;
						}
						else if (sceneWidth > 0.0f && sceneHeight > 0.0f && imageSize.x > 0.0f && imageSize.y > 0.0f)
						{
							const float u = (mousePos.x - imagePos.x) / imageSize.x;
							const float v = (mousePos.y - imagePos.y) / imageSize.y;
							const float mouseSceneX = u * sceneWidth;
							const float mouseSceneY = v * sceneHeight;

							const DirectX::SimpleMath::Vector2 mouseScene = { mouseSceneX, mouseSceneY };
							const DirectX::SimpleMath::Vector2 d = mouseScene - s_uiResizeStartPivotScene;
							const float localX = d.x * s_uiResizeRightDir.x + d.y * s_uiResizeRightDir.y
								+ s_uiResizeStartSizeScene.x * s_uiResizePivot.x;
							const float localY = d.x * s_uiResizeUpDir.x + d.y * s_uiResizeUpDir.y
								+ s_uiResizeStartSizeScene.y * s_uiResizePivot.y;

							float newW = s_uiResizeStartSizeScene.x;
							float newH = s_uiResizeStartSizeScene.y;
					DirectX::SimpleMath::Vector2 fixedCornerLocal = {};
							switch (s_uiResizeHandle)
							{
							case 0: // TL
								newW = s_uiResizeStartSizeScene.x - localX;
								newH = s_uiResizeStartSizeScene.y - localY;
						fixedCornerLocal = { newW, newH };
								break;
							case 1: // TR
								newW = localX;
								newH = s_uiResizeStartSizeScene.y - localY;
						fixedCornerLocal = { 0.0f, newH };
								break;
							case 2: // BR
								newW = localX;
								newH = localY;
						fixedCornerLocal = { 0.0f, 0.0f };
								break;
							case 3: // BL
								newW = s_uiResizeStartSizeScene.x - localX;
								newH = localY;
						fixedCornerLocal = { newW, 0.0f };
								break;
							}

							const float minSizeScene = 1.0f;
							newW = std::max(newW, minSizeScene);
							newH = std::max(newH, minSizeScene);

							const DirectX::SimpleMath::Vector2 pivotLocal = {
								newW * s_uiResizePivot.x,
								newH * s_uiResizePivot.y
							};
							const DirectX::SimpleMath::Vector2 newPivotScene =
								s_uiResizeFixedCornerScene
						- s_uiResizeRightDir * (fixedCornerLocal.x - pivotLocal.x)
						- s_uiResizeUpDir * (fixedCornerLocal.y - pivotLocal.y);

							const DirectX::SimpleMath::Vector2 newPivotCanvas = {
								s_uiResizeScaleToScene.x > 0.0f ? (newPivotScene.x - s_uiResizeSceneOffset.x) / s_uiResizeScaleToScene.x : newPivotScene.x,
								s_uiResizeScaleToScene.y > 0.0f ? (newPivotScene.y - s_uiResizeSceneOffset.y) / s_uiResizeScaleToScene.y : newPivotScene.y
							};

							const DirectX::SimpleMath::Vector2 delta = newPivotCanvas - s_uiResizeStartAnchorCenter;
							const DirectX::SimpleMath::Vector2 anchoredPos = {
								delta.x * s_uiResizeParentRight.x + delta.y * s_uiResizeParentRight.y,
								delta.x * s_uiResizeParentUp.x + delta.y * s_uiResizeParentUp.y
							};
							rectTr->SetAnchoredPosition(anchoredPos);

							DirectX::SimpleMath::Vector2 sizeCanvas = {
								s_uiResizeScaleToScene.x > 0.0f ? (newW / s_uiResizeScaleToScene.x) : newW,
								s_uiResizeScaleToScene.y > 0.0f ? (newH / s_uiResizeScaleToScene.y) : newH
							};
							const auto worldScale = rectTr->GetWorldScale();
							if (std::abs(worldScale.x) > 1e-4f) sizeCanvas.x /= worldScale.x;
							if (std::abs(worldScale.y) > 1e-4f) sizeCanvas.y /= worldScale.y;

							rectTr->SetSizeDelta(sizeCanvas - s_uiResizeStartAnchorSpan);
						}
					}
					else if (s_uiResizeActive && s_uiResizeTarget != rectTr)
					{
						s_uiResizeActive = false;
					}

					for (int i = 0; i < 4; ++i)
					{
						const bool hot = (i == hoveredHandle) || (s_uiResizeActive && s_uiResizeHandle == i && s_uiResizeTarget == rectTr);
						const ImU32 color = hot ? handleColorHot : handleColor;
						drawList->AddCircleFilled(handleScreen[i], handleRadius, color);
					}
				}

				const bool allowUiDrag = showUiHandles && !s_uiResizeActive;
				if (allowUiDrag)
				{
					ImVec2 mousePos = ImGui::GetMousePos();
					const bool mouseInImage = mousePos.x >= imagePos.x && mousePos.x <= imageMax.x
						&& mousePos.y >= imagePos.y && mousePos.y <= imageMax.y;
					const bool mouseDown = ImGui::IsMouseDown(ImGuiMouseButton_Left);

					if (!s_uiDragActive && mouseInImage && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
					{
						// 클릭한 지점이 선택된 UI 내부인지 확인
						const DirectX::SimpleMath::Vector2 mouseScene = {
							(mousePos.x - imagePos.x) * (sceneWidth / imageSize.x),
							(mousePos.y - imagePos.y) * (sceneHeight / imageSize.y)
						};
						const DirectX::SimpleMath::Vector2 d = mouseScene - pivotPosScene;
						const float localX = d.x * rightDir.x + d.y * rightDir.y;
						const float localY = d.x * upDir.x + d.y * upDir.y;
						const float u = (rectScene.z > 1e-6f) ? (localX / rectScene.z + pivot.x) : pivot.x;
						const float v = (rectScene.w > 1e-6f) ? (localY / rectScene.w + pivot.y) : pivot.y;
						const bool inside = (u >= 0.0f && u <= 1.0f && v >= 0.0f && v <= 1.0f);
						if (inside)
						{
							s_uiDragActive = true;
							s_uiDragTarget = rectTr;
							s_uiDragStartMouseScene = mouseScene;
							s_uiDragStartPivotScene = pivotPosScene;
							DirectX::SimpleMath::Vector2 anchorCenter;
							DirectX::SimpleMath::Vector2 anchorSpan;
							ComputeAnchorData(rectTr, canvasInfo.canvasSize, anchorCenter, anchorSpan);
							s_uiDragStartAnchorCenter = anchorCenter;
							ComputeParentBasis(rectTr, s_uiDragParentRight, s_uiDragParentUp);
							s_uiDragScaleToScene = canvasInfo.scaleToScene;
							s_uiDragSceneOffset = canvasInfo.sceneOffset;
						}
					}

					if (s_uiDragActive && s_uiDragTarget == rectTr)
					{
						if (!mouseDown)
						{
							s_uiDragActive = false;
						}
						else if (sceneWidth > 0.0f && sceneHeight > 0.0f && imageSize.x > 0.0f && imageSize.y > 0.0f)
						{
							const DirectX::SimpleMath::Vector2 mouseScene = {
								(mousePos.x - imagePos.x) * (sceneWidth / imageSize.x),
								(mousePos.y - imagePos.y) * (sceneHeight / imageSize.y)
							};
							const DirectX::SimpleMath::Vector2 deltaScene = mouseScene - s_uiDragStartMouseScene;
							const DirectX::SimpleMath::Vector2 newPivotScene = s_uiDragStartPivotScene + deltaScene;
							const DirectX::SimpleMath::Vector2 newPivotCanvas = {
								s_uiDragScaleToScene.x > 0.0f ? (newPivotScene.x - s_uiDragSceneOffset.x) / s_uiDragScaleToScene.x : newPivotScene.x,
								s_uiDragScaleToScene.y > 0.0f ? (newPivotScene.y - s_uiDragSceneOffset.y) / s_uiDragScaleToScene.y : newPivotScene.y
							};
							const DirectX::SimpleMath::Vector2 delta = newPivotCanvas - s_uiDragStartAnchorCenter;
							const DirectX::SimpleMath::Vector2 anchoredPos = {
								delta.x * s_uiDragParentRight.x + delta.y * s_uiDragParentRight.y,
								delta.x * s_uiDragParentUp.x + delta.y * s_uiDragParentUp.y
							};
							rectTr->SetAnchoredPosition(anchoredPos);
						}
					}
				}

				if (sceneWidth > 0.0f && sceneHeight > 0.0f && hasUiGizmo)
				{
					// UI는 화면 좌표계(좌상단 원점)를 사용하므로 이에 맞는 투영을 사용합니다.
					projMat = Matrix::CreateOrthographicOffCenter(
						0.0f, sceneWidth,
						sceneHeight, 0.0f,
						-1.0f, 1.0f);
				}

				const auto currentEuler = rectTr->GetWorldEulerRotation();
				const float currentRotZ = DirectX::XMConvertToRadians(currentEuler.z);
				Matrix modelMat =
					Matrix::CreateScale(rectScene.z, rectScene.w, 1.0f) *
					Matrix::CreateRotationZ(currentRotZ) *
					Matrix::CreateTranslation(pivotPosScene.x, pivotPosScene.y, 0.0f);

				float* viewPtr = &viewMat.m[0][0];
				float* projPtr = &projMat.m[0][0];
				float* modelPtr = &modelMat.m[0][0];

				if (hasUiGizmo)
					ImGuizmo::Manipulate(viewPtr, projPtr, op, ImGuizmo::LOCAL, modelPtr, nullptr, useSnap ? snapValue : nullptr);

				if (ImGuizmo::IsUsing())
				{
					Vector3 t, s;
					Quaternion r;
					modelMat.Decompose(s, r, t);

					s.x = std::abs(s.x);
					s.y = std::abs(s.y);

					const DirectX::SimpleMath::Vector2 newPivotScene = {
						t.x,
						t.y
					};
					const DirectX::SimpleMath::Vector2 newPivotCanvas = {
						canvasInfo.scaleToScene.x > 0.0f ? (newPivotScene.x - canvasInfo.sceneOffset.x) / canvasInfo.scaleToScene.x : newPivotScene.x,
						canvasInfo.scaleToScene.y > 0.0f ? (newPivotScene.y - canvasInfo.sceneOffset.y) / canvasInfo.scaleToScene.y : newPivotScene.y
					};

					DirectX::SimpleMath::Vector2 anchorCenter;
					DirectX::SimpleMath::Vector2 anchorSpan;
					ComputeAnchorData(rectTr, canvasInfo.canvasSize, anchorCenter, anchorSpan);

					DirectX::SimpleMath::Vector2 parentRight;
					DirectX::SimpleMath::Vector2 parentUp;
					ComputeParentBasis(rectTr, parentRight, parentUp);
					const DirectX::SimpleMath::Vector2 delta = newPivotCanvas - anchorCenter;
					const DirectX::SimpleMath::Vector2 anchoredPos = {
						delta.x * parentRight.x + delta.y * parentRight.y,
						delta.x * parentUp.x + delta.y * parentUp.y
					};
					rectTr->SetAnchoredPosition(anchoredPos);

					if (op == ImGuizmo::ROTATE)
					{
						const auto newEulerRad = r.ToEuler();
						DirectX::SimpleMath::Vector3 newEulerDeg = {
							DirectX::XMConvertToDegrees(newEulerRad.x),
							DirectX::XMConvertToDegrees(newEulerRad.y),
							DirectX::XMConvertToDegrees(newEulerRad.z)
						};
						DirectX::SimpleMath::Vector3 appliedEuler = currentEuler;
						appliedEuler.z = newEulerDeg.z;
						rectTr->SetWorldEulerRotation(appliedEuler);
					}

					if (op == ImGuizmo::SCALE)
					{
						const float baseW = rectScene.z;
						const float baseH = rectScene.w;
						const float scaleX = (baseW > 1e-6f) ? (s.x / baseW) : 1.0f;
						const float scaleY = (baseH > 1e-6f) ? (s.y / baseH) : 1.0f;

						auto worldScale = rectTr->GetWorldScale();
						worldScale.x *= scaleX;
						worldScale.y *= scaleY;
						rectTr->SetWorldScale(worldScale);
					}
				}
			}
		}
		else if ((int)m_guizmoOperation != 0)
		{
			gizmoDrawn = true;

			ImGuizmo::SetRect(imagePos.x, imagePos.y, imageSize.x, imageSize.y);

			float snapValue[3] = { 0.f, 0.f, 0.f };
			bool useSnap = ImGui::GetIO().KeyCtrl;

			if (useSnap)
			{
				if (m_guizmoOperation == ImGuizmo::TRANSLATE)
					snapValue[0] = snapValue[1] = snapValue[2] = 0.5f; // 0.5 단위 이동
				else if (m_guizmoOperation == ImGuizmo::ROTATE)
					snapValue[0] = snapValue[1] = snapValue[2] = 15.0f; // 15도 단위 회전
				else if (m_guizmoOperation == ImGuizmo::SCALE)
					snapValue[0] = snapValue[1] = snapValue[2] = 0.1f; // 0.1 단위 스케일
			}

			ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
			ImGuizmo::SetOrthographic(m_pCam->IsOrthographic());

			auto viewMat = m_pCam->GetViewMatrix();
			auto projMat = m_pCam->GetProjMatrix();

			auto modelMat = Matrix::Identity;

			if (g_selectedGameObject.IsValid() && !g_selectedGameObject->IsDestroyed()
				&& g_selectedGameObject->GetTransform().IsValid() && !g_selectedGameObject->GetTransform()->IsDestroyed())
				modelMat = g_selectedGameObject->GetTransform()->GetWorldMatrix(); // 값이라도 로컬에 저장

			float* viewPtr = &viewMat.m[0][0];
			float* projPtr = &projMat.m[0][0];
			float* modelPtr = &modelMat.m[0][0];

			ImGuizmo::Manipulate(viewPtr, projPtr, m_guizmoOperation, m_guizmoMode, modelPtr, NULL, useSnap ? snapValue : NULL);

			if (ImGuizmo::IsUsing())
			{
				Vector3 t, s;
				Quaternion r;
				modelMat.Decompose(s, r, t);

				auto tr = g_selectedGameObject->GetTransform();

				// 3. SnapToZero 적용 (미세한 오차 제거)
				auto SnapToZero = [](float& v, float eps = 1e-4f) {
					if (std::abs(v) < eps) v = 0.0f;
					};

				if (m_guizmoOperation == ImGuizmo::ROTATE)
				{
					s = tr->GetWorldScale();
				}
				else
				{
					SnapToZero(s.x); SnapToZero(s.y); SnapToZero(s.z);
				}

				SnapToZero(t.x); SnapToZero(t.y); SnapToZero(t.z);

				r.Normalize();

				tr->SetWorldPosition(t);
				tr->SetWorldRotation(r);
				tr->SetWorldScale(s);

				if (g_editor_scene_playing)
				{
					auto rbPtr = g_selectedGameObject->GetComponent<RigidBodyComponent>();
					if (rbPtr.IsValid())
					{
						if (rbPtr->GetKinematic())
							rbPtr->SetKinematicTarget(t, r);
						else
							rbPtr->Editor_changeTrans(t, r);
					}
				}
			}
		}
	}

	bool toolbuttonHovered = false;
	{
		auto buttonsize = ImVec2(0, 0);
		auto padding = ImVec2{ 10,10 };
		auto handing = (int)m_guizmoOperation == 0;
		auto moving = m_guizmoOperation == ImGuizmo::OPERATION::TRANSLATE;
		auto rotating = m_guizmoOperation == ImGuizmo::OPERATION::ROTATE;
		auto scaling = m_guizmoOperation == ImGuizmo::OPERATION::SCALE;
		auto local = m_guizmoMode == ImGuizmo::MODE::LOCAL;
		auto world = m_guizmoMode == ImGuizmo::MODE::WORLD;
		const bool prev2DMode = m_ui2DMode;

		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.9f, 0.9f, 0.9f, 1.0f));

		ImGui::SetCursorPos(scenecornerpos + padding);
		// Hand/Rect 버튼 (Q) - UI 편집 모드에서는 Rect 표시
		ImGui::BeginDisabled(handing);
		const char* handLabel = u8"\uf256 hand";
		const char* rectLabel = u8"\uf0c8 rect";
		const char* primaryLabel = m_ui2DMode ? rectLabel : handLabel;
		if (ImGui::Button(primaryLabel, buttonsize)) // 폰트어썸 아이콘
		{
			m_guizmoOperation = (ImGuizmo::OPERATION)0;
		}
		if(ImGui::IsItemHovered())
			toolbuttonHovered = true;
		ImGui::EndDisabled();
		ImGui::SameLine();
		// Move 버튼
		ImGui::BeginDisabled(moving);
		if (ImGui::Button(u8"\uf047 move", buttonsize))
		{
			m_guizmoOperation = ImGuizmo::TRANSLATE;
		}
		if (ImGui::IsItemHovered())
			toolbuttonHovered = true;
		ImGui::EndDisabled();

		ImGui::SameLine();

		// Rotate 버튼
		ImGui::BeginDisabled(rotating);
		if (ImGui::Button(u8"\uf2f1 rotate", buttonsize))
		{
			m_guizmoOperation = ImGuizmo::ROTATE;
		}
		if (ImGui::IsItemHovered())
			toolbuttonHovered = true;
		ImGui::EndDisabled();

		ImGui::SameLine();

		// Scale 버튼
		ImGui::BeginDisabled(scaling);
		if (ImGui::Button(u8"\uf31e scale", buttonsize))
		{
			m_guizmoOperation = ImGuizmo::SCALE;
		}
		if (ImGui::IsItemHovered())
			toolbuttonHovered = true;
		ImGui::EndDisabled();

		ImGui::SameLine();

		// 구분선
		ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);

		ImGui::SameLine();

		// Local 버튼
		ImGui::BeginDisabled(local);
		if (ImGui::Button(u8"\uf1b2 local", buttonsize))
		{
			m_guizmoMode = ImGuizmo::LOCAL;
		}
		if (ImGui::IsItemHovered())
			toolbuttonHovered = true;
		ImGui::EndDisabled();

		ImGui::SameLine();

		// World 버튼
		ImGui::BeginDisabled(world);
		if (ImGui::Button(u8"\uf0ac world", buttonsize))
		{
			m_guizmoMode = ImGuizmo::WORLD;
		}
		if (ImGui::IsItemHovered())
			toolbuttonHovered = true;
		ImGui::EndDisabled();

		ImGui::SameLine();
		ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
		ImGui::SameLine();

		const bool uiColorPushed = m_ui2DMode;
		if (uiColorPushed)
		{
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.7f, 0.25f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.35f, 0.8f, 0.35f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
		}
		if (ImGui::Button("UI", buttonsize))
		{
			m_ui2DMode = !m_ui2DMode;
		}
		if (ImGui::IsItemHovered())
			toolbuttonHovered = true;

		if(uiColorPushed)
			ImGui::PopStyleColor(3);

		// --- 카메라 설정 팝업 버튼 추가 ---
		ImGui::SameLine();
		ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
		ImGui::SameLine();

		if (ImGui::Button(u8"\uf0ad Camera Settings")) // 폰트어썸 렌치(wrench) 아이콘 사용 예시
		{
			ImGui::OpenPopup("CameraSettingsPopup");
		}
		if (ImGui::IsItemHovered())
			toolbuttonHovered = true;
		ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 10.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 8.0f));

		// 팝업 창 정의
		if (ImGui::BeginPopup("CameraSettingsPopup"))
		{
			float fov = m_pCam->GetFOV();
			float n = m_pCam->GetNearPlane();
			float f = m_pCam->GetFarPlane();
			bool ortho = m_pCam->IsOrthographicTarget();

			// 컨트롤 간의 간격을 위해 ItemSpacing도 조절하고 싶다면 추가 가능
			if (ImGui::Checkbox("Orthographic", &ortho)) m_pCam->SetOrthographic(ortho);
			if (!ortho)
			{
				if (ImGui::DragFloat("FOV", &fov, 0.5f, 10.0f, 120.0f)) m_pCam->SetFOV(fov);
			}
			else
			{
				float orthoSize = m_pCam->GetOrthoSize();
				if (ImGui::DragFloat("Ortho Size", &orthoSize, 0.1f, 0.1f, 1000.0f)) m_pCam->SetOrthoSize(orthoSize);
			}
			if (ImGui::DragFloat("Near", &n, 0.01f, 0.01f, 10.0f)) m_pCam->SetNearPlane(n);
			if (ImGui::DragFloat("Far", &f, 1.0f, 10.0f, 10000.0f)) m_pCam->SetFarPlane(f);

			ImGui::EndPopup();
		}

		if (prev2DMode != m_ui2DMode && m_pCam)
		{
			if (m_ui2DMode)
			{
				m_savedCamPos = m_pCam->GetPosition();
				m_savedCamRot = m_pCam->GetRotation();
				m_savedOrthoSize = m_pCam->GetOrthoSize();
				m_savedOrthoTarget = m_pCam->IsOrthographicTarget();
				m_hasSaved2DState = true;

				auto pos = m_pCam->GetPosition();
				pos.z = m_ui2DCameraDistance;
				m_pCam->SetPosition(pos);
				m_pCam->SetEulerRotation({ 0.0f, 0.0f, 0.0f });
				m_pCam->SetOrthographic(true);
				m_pCam->SyncInputState();
			}
			else if (m_hasSaved2DState)
			{
				m_pCam->SetPosition(m_savedCamPos);
				m_pCam->SetRotation(m_savedCamRot);
				m_pCam->SetOrthoSize(m_savedOrthoSize);
				m_pCam->SetOrthographic(m_savedOrthoTarget);
				m_pCam->SyncInputState();
			}
		}
		ImGui::PopStyleVar(2);
		// ----------------------------------

		ImGui::PopStyleColor(3);
	}

	bool viewGizmoUsing = false;
	if (imageSize.x > 0.0f && imageSize.y > 0.0f)
	{
		const float gizmoScale = 1.0f;
		const float gizmoSize = 64.0f * gizmoScale;
		const float gizmoPadding = 10.0f * gizmoScale;
		const float axisLength = gizmoSize * 0.35f;
		const float circleRadius = gizmoSize * 0.12f;
		const float centerRadius = gizmoSize * 0.08f;

		ImVec2 gizmoCenter = ImVec2(
			imageMax.x - gizmoPadding - gizmoSize * 0.5f,
			imagePos.y + gizmoPadding + gizmoSize * 0.5f);
		ImVec2 gizmoMin = ImVec2(gizmoCenter.x - gizmoSize * 0.5f, gizmoCenter.y - gizmoSize * 0.5f);
		ImVec2 gizmoMax = ImVec2(gizmoCenter.x + gizmoSize * 0.5f, gizmoCenter.y + gizmoSize * 0.5f);
		const ImVec2 centerHalfSize = ImVec2(centerRadius * 0.75f, centerRadius * 0.75f);
		const ImVec2 centerMin = ImVec2(gizmoCenter.x - centerHalfSize.x, gizmoCenter.y - centerHalfSize.y);
		const ImVec2 centerMax = ImVec2(gizmoCenter.x + centerHalfSize.x, gizmoCenter.y + centerHalfSize.y);

		ImDrawList* drawList = ImGui::GetWindowDrawList();
		drawList->AddRectFilled(gizmoMin, gizmoMax, IM_COL32(20, 20, 20, 100), 6.0f);
		Matrix camWorld = m_pCam->GetTransformMatrix();
		Vector3 camRight = camWorld.Right();
		Vector3 camUp = camWorld.Up();
		Vector3 camForward = camWorld.Forward();

		struct AxisWidget
		{
			int index = 0;
			ImVec2 dir2D = {};
			float depth = 0.0f;
			ImU32 color = 0;
			const char* label = "";
			ImVec2 endPos = {};
			Vector3 axisWorld = Vector3::Zero;
			bool filled = true;
			float lineLength = 0.0f;
		};

		const ImU32 axisColors[3] =
		{
			IM_COL32(230, 70, 70, 255),
			IM_COL32(110, 230, 110, 255),
			IM_COL32(80, 140, 230, 255)
		};
		const char* axisLabels[3] = { "X", "Y", "Z" };

		AxisWidget axes[6];
		int axisCount = 0;
		for (int i = 0; i < 3; ++i)
		{
			Vector3 baseAxis = Vector3::Zero;
			if (i == 0) baseAxis = Vector3(1.0f, 0.0f, 0.0f);
			else if (i == 1) baseAxis = Vector3(0.0f, 1.0f, 0.0f);
			else baseAxis = Vector3(0.0f, 0.0f, 1.0f);

			for (int sign = 0; sign < 2; ++sign)
			{
				const float s = sign == 0 ? 1.0f : -1.0f;
				Vector3 axisWorld = baseAxis * s;

				Vector3 axisView = Vector3(
					axisWorld.Dot(camRight),
					axisWorld.Dot(camUp),
					axisWorld.Dot(camForward));
				ImVec2 dir2 = ImVec2(axisView.x, -axisView.y);
				float planar = std::sqrt(dir2.x * dir2.x + dir2.y * dir2.y);
				if (planar > 1e-5f)
				{
					dir2.x /= planar;
					dir2.y /= planar;
				}
				else
				{
					dir2.x = 0.0f;
					dir2.y = 0.0f;
				}

				float depthT = (axisView.z + 1.0f) * 0.5f;
				depthT = std::clamp(depthT, 0.0f, 1.0f);
				float length = axisLength * planar * (0.6f + 0.4f * depthT);

				AxisWidget& axis = axes[axisCount++];
				axis.index = i;
				axis.dir2D = dir2;
				axis.depth = axisView.z;
				axis.color = axisColors[i];
				axis.label = sign == 0 ? axisLabels[i] : "";
				axis.endPos = ImVec2(gizmoCenter.x + dir2.x * length, gizmoCenter.y + dir2.y * length);
				axis.axisWorld = axisWorld;
				axis.filled = (sign == 0);
				axis.lineLength = length;
			}
		}

		const float axisFadeRange = 0.2f;
		const float backAxisAlpha = 0.425f;
		auto calcAxisFade = [&](float depth)
		{
			const float start = -axisFadeRange;
			const float end = axisFadeRange;
			float t = (depth - start) / (end - start);
			t = std::clamp(t, 0.0f, 1.0f);
			t = t * t * (3.0f - 2.0f * t);
			return backAxisAlpha + (1.0f - backAxisAlpha) * t;
		};

		auto drawAxisLine = [&](const AxisWidget& axis)
		{
			float alpha = calcAxisFade(axis.depth);
			float lineAlpha = axis.filled ? 200.0f : 120.0f;
			ImU32 lineColor = IM_COL32(
				(int)(ImGui::ColorConvertU32ToFloat4(axis.color).x * 255.0f),
				(int)(ImGui::ColorConvertU32ToFloat4(axis.color).y * 255.0f),
				(int)(ImGui::ColorConvertU32ToFloat4(axis.color).z * 255.0f),
				(int)(alpha * lineAlpha));

			float lineLen = axis.lineLength - circleRadius;
			if (lineLen > 0.0f)
			{
				ImVec2 lineEnd = ImVec2(
					gizmoCenter.x + axis.dir2D.x * lineLen,
					gizmoCenter.y + axis.dir2D.y * lineLen);
				drawList->AddLine(gizmoCenter, lineEnd, lineColor, 2.0f);
			}
		};

		auto drawAxisCircleAndLabel = [&](const AxisWidget& axis)
		{
			float alpha = calcAxisFade(axis.depth);
			ImU32 circleColor = IM_COL32(
				(int)(ImGui::ColorConvertU32ToFloat4(axis.color).x * 255.0f),
				(int)(ImGui::ColorConvertU32ToFloat4(axis.color).y * 255.0f),
				(int)(ImGui::ColorConvertU32ToFloat4(axis.color).z * 255.0f),
				(int)(alpha * 255.0f));
			if (axis.filled)
			{
				drawList->AddCircleFilled(axis.endPos, circleRadius, circleColor);
			}
			else
			{
				drawList->AddCircle(axis.endPos, circleRadius * 0.95f, circleColor, 0, 2.0f);
			}

			if (axis.label && axis.label[0] != '\0')
			{
				ImVec2 textSize = ImGui::CalcTextSize(axis.label);
				drawList->AddText(ImVec2(axis.endPos.x - textSize.x * 0.5f, axis.endPos.y - textSize.y * 0.5f),
					IM_COL32(10, 10, 10, (int)(alpha * 220.0f)), axis.label);
			}
		};

		AxisWidget ordered[6];
		for (int i = 0; i < axisCount; ++i)
			ordered[i] = axes[i];
		std::sort(ordered, ordered + axisCount, [](const AxisWidget& a, const AxisWidget& b)
		{
			return a.depth < b.depth;
		});

		// Lines always behind center toggle
		for (int i = 0; i < axisCount; ++i)
			drawAxisLine(ordered[i]);

		// Back axes (faded) circles/labels behind center toggle
		for (int i = 0; i < axisCount; ++i)
		{
			if (ordered[i].depth < 0.0f)
				drawAxisCircleAndLabel(ordered[i]);
		}

		// center toggle (rounded rect) drawn after back axes so it stays visible
		drawList->AddRectFilled(centerMin, centerMax, IM_COL32(245, 245, 245, 240), 2.5f * gizmoScale);

		// Front axes circles/labels above center toggle
		for (int i = 0; i < axisCount; ++i)
		{
			if (ordered[i].depth >= 0.0f)
				drawAxisCircleAndLabel(ordered[i]);
		}

		ImVec2 mousePos = ImGui::GetMousePos();
		bool anyHovered = false;
		int hoveredAxis = -1;
		for (int i = 0; i < axisCount; ++i)
		{
			ImVec2 delta = ImVec2(mousePos.x - axes[i].endPos.x, mousePos.y - axes[i].endPos.y);
			if (delta.x * delta.x + delta.y * delta.y <= circleRadius * circleRadius)
			{
				hoveredAxis = i;
				anyHovered = true;
				break;
			}
		}

		bool centerHovered = mousePos.x >= centerMin.x && mousePos.x <= centerMax.x
			&& mousePos.y >= centerMin.y && mousePos.y <= centerMax.y;
		if (centerHovered)
			anyHovered = true;

		if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
		{
			if (centerHovered)
			{
				m_pCam->ToggleProjectionMode();
			}
			else if (hoveredAxis != -1)
			{
				Vector3 axisDir = axes[hoveredAxis].axisWorld;

				if (!m_hasViewGizmoPivot)
				{
					Matrix camWorld = m_pCam->GetTransformMatrix();
					Vector3 forward = camWorld.Forward();
					const float initDistance = std::max(0.1f, m_viewGizmoDistance);
					m_viewGizmoPivot = m_pCam->GetPosition() + forward * initDistance;
					m_hasViewGizmoPivot = true;
				}

				const Vector3 pivot = m_viewGizmoPivot;
				const Vector3 target = pivot;
				const float viewDistance = std::max(0.1f, m_viewGizmoDistance);
				Vector3 eye = pivot + axisDir * viewDistance;

				Vector3 up = Vector3(0.0f, 1.0f, 0.0f);
				if (std::abs(axisDir.y) > 0.9f)
				{
					up = Vector3(0.0f, 0.0f, 1.0f);
				}

				DirectX::XMVECTOR eyeV = DirectX::XMVectorSet(eye.x, eye.y, eye.z, 1.0f);
				DirectX::XMVECTOR targetV = DirectX::XMVectorSet(target.x, target.y, target.z, 1.0f);
				DirectX::XMVECTOR upV = DirectX::XMVectorSet(up.x, up.y, up.z, 0.0f);

				Matrix newView;
				DirectX::XMStoreFloat4x4(&newView, DirectX::XMMatrixLookAtLH(eyeV, targetV, upV));
				Matrix invView = newView.Invert();
				m_viewGizmoTransition.active = true;
				m_viewGizmoTransition.elapsed = 0.0f;
				m_viewGizmoTransition.startPos = m_pCam->GetPosition();
				m_viewGizmoTransition.startRot = m_pCam->GetRotation();
				m_viewGizmoTransition.targetPos = invView.Translation();
				m_viewGizmoTransition.targetRot = Quaternion::CreateFromRotationMatrix(invView);
			}
		}

		viewGizmoUsing = anyHovered && ImGui::IsMouseDown(ImGuiMouseButton_Left);
		toolbuttonHovered = toolbuttonHovered || anyHovered || ImGui::IsMouseHoveringRect(gizmoMin, gizmoMax);
	}

	toolbuttonHovered = toolbuttonHovered || uiResizeHovered;

	// 씬 뷰 픽킹 (좌클릭)
	{
	const bool gizmoBlocking = (gizmoDrawn && (ImGuizmo::IsOver() || ImGuizmo::IsUsing())) || viewGizmoUsing || uiResizeUsing || s_uiDragActive;
		ImVec2 mousePos = ImGui::GetMousePos();
		bool mouseInImage = mousePos.x >= imagePos.x && mousePos.x <= imageMax.x
			&& mousePos.y >= imagePos.y && mousePos.y <= imageMax.y;
		if (m_isHovered && mouseInImage && ImGui::IsMouseClicked(ImGuiMouseButton_Left)
			&& !gizmoBlocking
			&& !toolbuttonHovered)
		{
			ImVec2 imageSize = ImVec2(imageMax.x - imagePos.x, imageMax.y - imagePos.y);
			if (imageSize.x > 0.0f && imageSize.y > 0.0f)
			{
				float u = (mousePos.x - imagePos.x) / imageSize.x;
				float v = (mousePos.y - imagePos.y) / imageSize.y;
				const float sceneX = u * static_cast<float>(m_width);
				const float sceneY = v * static_cast<float>(m_height);

				if (m_ui2DMode)
				{
					ObjPtr<GameObject> picked = nullptr;
					const auto& canvases = RenderManager::Get().GetCanvases();
					for (auto* canvas : canvases)
					{
						if (!canvas || !canvas->IsActiveAndEnabled())
							continue;

						std::vector<ObjPtr<Graphic>> graphics;
						graphics.reserve(canvas->GetGraphics().size());
						for (auto& graphic : canvas->GetGraphics())
						{
							if (!graphic.IsValid() || !graphic->IsActiveAndEnabled())
								continue;
							graphics.push_back(graphic);
						}

						std::stable_sort(graphics.begin(), graphics.end(),
							[](const ObjPtr<Graphic>& a, const ObjPtr<Graphic>& b)
							{
								return a->GetRenderOrder() < b->GetRenderOrder();
							});

						const auto canvasInfo = GetCanvasInfo(canvas, static_cast<float>(m_width), static_cast<float>(m_height));
						for (auto& graphic : graphics)
						{
							auto rectTr = graphic->GetRectTransform();
							if (!rectTr.IsValid())
								continue;

							auto rectCanvas = rectTr->GetRectInCanvas(canvasInfo.canvasSize);
							auto rectScene = DirectX::SimpleMath::Vector4(
								canvasInfo.sceneOffset.x + rectCanvas.x * canvasInfo.scaleToScene.x,
								canvasInfo.sceneOffset.y + rectCanvas.y * canvasInfo.scaleToScene.y,
								rectCanvas.z * canvasInfo.scaleToScene.x,
								rectCanvas.w * canvasInfo.scaleToScene.y);

							const auto pivot = rectTr->GetPivot();
							const auto worldRot = rectTr->GetWorldRotation();
							if (PointInRotatedRect(sceneX, sceneY, rectScene, pivot, worldRot))
							{
								picked = graphic->GetGameObject();
							}
							}
						}

					g_selectedGameObject = picked;
					if (picked.IsValid() && picked->GetTransform().IsValid() && picked->GetTransform()->GetParent() != nullptr)
						HierarchyWindow::Get().RequestExpandParentsForSelection();
				}
				else
				{
					int x = static_cast<int>(sceneX);
					int y = static_cast<int>(sceneY);

					if (x >= 0 && y >= 0 && x < m_width && y < m_height && m_pIdStagingTex)
					{
						m_cachedContext->CopyResource(m_pIdStagingTex.Get(), m_pIdTexture.Get());

						D3D11_MAPPED_SUBRESOURCE mapped = {};
						if (SUCCEEDED(m_cachedContext->Map(m_pIdStagingTex.Get(), 0, D3D11_MAP_READ, 0, &mapped)))
						{
							auto* row = reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(mapped.pData) + y * mapped.RowPitch);
							uint32_t pickedId = row[x];
							m_cachedContext->Unmap(m_pIdStagingTex.Get(), 0);

							if (pickedId == 0)
							{
								g_selectedGameObject = nullptr;
							}
							else
							{
								auto* renderer = RenderManager::Get().GetRendererById(pickedId - 1);
								if (renderer && renderer->GetGameObject().IsValid() && !renderer->GetGameObject()->IsDestroyed())
								{
									g_selectedGameObject = renderer->GetGameObject();
									if (g_selectedGameObject->GetTransform().IsValid() && g_selectedGameObject->GetTransform()->GetParent() != nullptr)
										HierarchyWindow::Get().RequestExpandParentsForSelection();
								}
								else
								{
									g_selectedGameObject = nullptr;
								}
							}
						}
					}
				}
			}
		}
	}
	m_blockCameraInput = viewGizmoUsing || m_viewGizmoTransition.active;
	ImGui::End();
	ImGui::PopStyleVar();
}
bool MMMEngine::Editor::SceneViewWindow::CreateRenderTargets(ID3D11Device* device, int width, int height)
{
	// 기존 리소스 해제
	m_pSceneTexture.Reset();
	m_pSceneRTV.Reset();
	m_pSceneSRV.Reset();
	m_pSceneDSV.Reset();
	m_pDepthStencilBuffer.Reset();
	m_pIdTexture.Reset();
	m_pIdRTV.Reset();
	m_pIdSRV.Reset();
	m_pIdStagingTex.Reset();
	m_pMaskTexture.Reset();
	m_pMaskRTV.Reset();
	m_pMaskSRV.Reset();

	m_width = width;
	m_height = height;

	// Render Target Texture 생성
	D3D11_TEXTURE2D_DESC textureDesc = {};
	textureDesc.Width = width;
	textureDesc.Height = height;
	textureDesc.MipLevels = 1;
	textureDesc.ArraySize = 1;
	textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	textureDesc.SampleDesc.Count = 1;
	textureDesc.SampleDesc.Quality = 0;
	textureDesc.Usage = D3D11_USAGE_DEFAULT;
	textureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	textureDesc.CPUAccessFlags = 0;
	textureDesc.MiscFlags = 0;

	HRESULT hr = device->CreateTexture2D(&textureDesc, nullptr, m_pSceneTexture.GetAddressOf());
	if (FAILED(hr))
	{
		OutputDebugStringA("Failed to create scene texture\n");
		return false;
	}

	// Render Target View 생성
	D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.Format = textureDesc.Format;
	rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
	rtvDesc.Texture2D.MipSlice = 0;

	hr = device->CreateRenderTargetView(m_pSceneTexture.Get(), &rtvDesc, m_pSceneRTV.GetAddressOf());
	if (FAILED(hr))
	{
		OutputDebugStringA("Failed to create RTV\n");
		return false;
	}

	// Shader Resource View 생성 (ImGui에서 사용)
	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = textureDesc.Format;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = 1;

	hr = device->CreateShaderResourceView(m_pSceneTexture.Get(), &srvDesc, m_pSceneSRV.GetAddressOf());
	if (FAILED(hr))
	{
		OutputDebugStringA("Failed to create SRV\n");
		return false;
	}

	// Depth Stencil Buffer 생성
	D3D11_TEXTURE2D_DESC depthDesc = {};
	depthDesc.Width = width;
	depthDesc.Height = height;
	depthDesc.MipLevels = 1;
	depthDesc.ArraySize = 1;
	depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	depthDesc.SampleDesc.Count = 1;
	depthDesc.SampleDesc.Quality = 0;
	depthDesc.Usage = D3D11_USAGE_DEFAULT;
	depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	depthDesc.CPUAccessFlags = 0;
	depthDesc.MiscFlags = 0;

	hr = device->CreateTexture2D(&depthDesc, nullptr, m_pDepthStencilBuffer.GetAddressOf());
	if (FAILED(hr))
	{
		OutputDebugStringA("Failed to create depth stencil buffer\n");
		return false;
	}

	// Depth Stencil View 생성
	D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
	dsvDesc.Format = depthDesc.Format;
	dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Texture2D.MipSlice = 0;

	hr = device->CreateDepthStencilView(m_pDepthStencilBuffer.Get(), &dsvDesc, m_pSceneDSV.GetAddressOf());
	if (FAILED(hr))
	{
		OutputDebugStringA("Failed to create DSV\n");
		return false;
	}

	// 카메라 aspect ratio 업데이트
	m_pCam->SetAspectRatio((float)width, (float)height);


	D3D11_TEXTURE2D_DESC idDesc = {};
	idDesc.Width = width;
	idDesc.Height = height;
	idDesc.MipLevels = 1;
	idDesc.ArraySize = 1;
	idDesc.Format = DXGI_FORMAT_R32_UINT;           // 핵심
	idDesc.SampleDesc.Count = 1;
	idDesc.SampleDesc.Quality = 0;
	idDesc.Usage = D3D11_USAGE_DEFAULT;
	idDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

	device->CreateTexture2D(&idDesc, nullptr, m_pIdTexture.GetAddressOf());
	device->CreateRenderTargetView(m_pIdTexture.Get(), nullptr, m_pIdRTV.GetAddressOf());
	device->CreateShaderResourceView(m_pIdTexture.Get(), nullptr, m_pIdSRV.GetAddressOf());

	D3D11_TEXTURE2D_DESC stagingDesc = idDesc;
	stagingDesc.Usage = D3D11_USAGE_STAGING;
	stagingDesc.BindFlags = 0;
	stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	stagingDesc.MiscFlags = 0;
	device->CreateTexture2D(&stagingDesc, nullptr, m_pIdStagingTex.GetAddressOf());

	D3D11_TEXTURE2D_DESC maskDesc = {};
	maskDesc.Width = width;
	maskDesc.Height = height;
	maskDesc.MipLevels = 1;
	maskDesc.ArraySize = 1;
	maskDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	maskDesc.SampleDesc.Count = 1;
	maskDesc.SampleDesc.Quality = 0;
	maskDesc.Usage = D3D11_USAGE_DEFAULT;
	maskDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

	device->CreateTexture2D(&maskDesc, nullptr, m_pMaskTexture.GetAddressOf());
	device->CreateRenderTargetView(m_pMaskTexture.Get(), nullptr, m_pMaskRTV.GetAddressOf());
	device->CreateShaderResourceView(m_pMaskTexture.Get(), nullptr, m_pMaskSRV.GetAddressOf());

	return true;
}

void MMMEngine::Editor::SceneViewWindow::ResizeRenderTarget(ID3D11Device* device, int width, int height)
{
	if (width > 0 && height > 0 && (width != m_width || height != m_height))
	{
		CreateRenderTargets(device, width, height);
	}
}
void MMMEngine::Editor::SceneViewWindow::RenderSceneToTexture(ID3D11DeviceContext* context)
{
	ID3D11ShaderResourceView* nullSRV = nullptr;
	context->PSSetShaderResources(0, 1, &nullSRV);

	RenderStateGuard guard(context); // 백업/복원만 담당

	ID3D11RenderTargetView* rtv = m_pSceneRTV.Get();
	ID3D11DepthStencilView* dsv = m_pSceneDSV.Get();

	// Viewport
	D3D11_VIEWPORT viewport{};
	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;
	viewport.Width = static_cast<float>(m_width);
	viewport.Height = static_cast<float>(m_height);
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	context->RSSetViewports(1, &viewport);

	// 셰이더 로딩 (프로젝트 로드 이후)
	if ((!m_pPickingVS || !m_pPickingPS || !m_pMaskPS || !m_pFullScreenVS || !m_pOutlinePS) &&
		!ResourceManager::Get().GetCurrentRootPath().empty())
	{
		if (!m_pPickingVS)
			m_pPickingVS = ResourceManager::Get().Load<VShader>(L"Shader/PBR/VS/StaticVertexShader.hlsl");
		if (!m_pPickingPS)
			m_pPickingPS = ResourceManager::Get().Load<PShader>(L"Shader/Editor/PickingPS.hlsl");
		if (!m_pMaskPS)
			m_pMaskPS = ResourceManager::Get().Load<PShader>(L"Shader/Editor/MaskPS.hlsl");
		if (!m_pFullScreenVS)
			m_pFullScreenVS = ResourceManager::Get().Load<VShader>(L"Shader/PP/FullScreenVS.hlsl");
		if (!m_pOutlinePS)
			m_pOutlinePS = ResourceManager::Get().Load<PShader>(L"Shader/Editor/OutlinePS.hlsl");
	}

	if (m_ui2DMode && m_pCam)
	{
		m_pCam->SetOrthographic(true);
	}

	m_pCam->UpdateProjectionBlend();
	if (m_isFocused && !m_blockCameraInput)
		m_pCam->InputUpdate((int)m_guizmoOperation);
	m_pCam->UpdateState();
	if (m_ui2DMode && m_pCam)
	{
		auto pos = m_pCam->GetPosition();
		pos.z = m_ui2DCameraDistance;
		m_pCam->SetPosition(pos);
		m_pCam->SetEulerRotation({ 0.0f, 0.0f, 0.0f });
	}

	auto view = m_pCam->GetViewMatrix();
	auto proj = m_pCam->GetProjMatrix();
	auto ortho = m_pCam->IsOrthographic();

	RenderManager::Get().SetViewMatrix(view);
	RenderManager::Get().SetProjMatrix(proj);
	RenderManager::Get().SetOrtho(ortho);

	// ID 텍스쳐 렌더링
	if (m_pPickingVS && m_pPickingPS && m_pPickingIdBuffer)
	{
		context->OMSetRenderTargets(1, m_pIdRTV.GetAddressOf(), dsv);
		float idClear[4] = { 0, 0, 0, 0 };
		context->ClearRenderTargetView(m_pIdRTV.Get(), idClear);
		context->ClearDepthStencilView(dsv, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
		context->RSSetState(m_states->CullNone());

		RenderManager::Get().RenderPickingIds(
			m_pPickingPS->m_pPShader.Get(),
			m_pPickingIdBuffer.Get());
	}

	// Scene 렌더링
	context->OMSetDepthStencilState(nullptr, 0);
	context->OMSetRenderTargets(1, &rtv, dsv);
	float clearColor[4] = { 0.1f, 0.1f, 0.1f, 1.0f };
	context->ClearRenderTargetView(rtv, clearColor);
	context->ClearDepthStencilView(dsv, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

	if (!m_ui2DMode)
		m_pGridRenderer->Render(context, *m_pCam);

	RenderManager::Get().RenderOnlyRenderer();

	// 디버그 드로잉
	if (m_enableDebugDraw)
	{
		if (!m_enableDebugDrawZbuffer)
		{
			context->OMSetBlendState(m_states->Opaque(), nullptr, 0xFFFFFFFF);
			context->OMSetDepthStencilState(m_states->DepthNone(), 0);
			context->RSSetState(m_states->CullNone());
		}

		m_effect->SetView(view);
		m_effect->SetProjection(proj);
		m_effect->Apply(context);
		context->IASetInputLayout(m_pDebugDrawIL.Get());

		m_batch->Begin();

		auto& sceneGameObjects = SceneManager::Get().GetAllGameObjectInCurrentScene();


		// 전체보기는 성능이 문제로 픽한게임오브젝트만
		if(g_selectedGameObject.IsValid())
		{
			auto& go = g_selectedGameObject;

			if (go->IsActiveInHierarchy())
			{
				auto& ColliderComponents = go->GetComponents<ColliderComponent>();

				for (auto& col : ColliderComponents)
				{
					if (!col.IsValid() && col->IsDestroyed())
					{
						continue;
					}
					auto desc = col->GetDebugShapeDesc();




					switch (desc.type)
					{
					case ColliderComponent::DebugColliderType::Unknown:
					{
						if (auto& meshCol = col.Cast<MeshColliderComponent>(); meshCol.IsValid())
						{
							auto make_rt_noscale = [](const Vector3& translation,
								const Quaternion& rotation) -> Matrix
								{
									return
										Matrix::CreateFromQuaternion(rotation) *
										Matrix::CreateTranslation(translation);
								};

							auto rt = make_rt_noscale(go->GetTransform()->GetLocalPosition(), go->GetTransform()->GetLocalRotation());

							if (auto convexMesh = meshCol->GetConvexMesh(); convexMesh)
							{
								auto verts = convexMesh->getVertices();
								auto ib = convexMesh->getIndexBuffer();
								physx::PxU32 polyCount = convexMesh->getNbPolygons();

								for (physx::PxU32 p = 0; p < polyCount; ++p)
								{
									physx::PxHullPolygon poly;
									if (!convexMesh->getPolygonData(p, poly)) continue;

									const physx::PxU32 n = poly.mNbVerts;
									const physx::PxU32 base = poly.mIndexBase;

									if (n < 3) continue;

									physx::PxU32 i0 = ib[base + 0];

									for (physx::PxU32 i = 1; i + 1 < n; ++i)
									{
										physx::PxU32 i1 = ib[base + i];
										physx::PxU32 i2 = ib[base + i + 1];

										if (i0 >= convexMesh->getNbVertices() ||
											i1 >= convexMesh->getNbVertices() ||
											i2 >= convexMesh->getNbVertices())
											continue;

										// verts[i0], verts[i1], verts[i2] 변환해서 DrawTriangle
										physx::PxVec3 v0 = verts[i0];
										physx::PxVec3 v1 = verts[i1];
										physx::PxVec3 v2 = verts[i2];

										XMVECTOR A = XMVector3TransformCoord(
											XMVectorSet(v0.x, v0.y, v0.z, 1.0f), rt);

										XMVECTOR B = XMVector3TransformCoord(
											XMVectorSet(v1.x, v1.y, v1.z, 1.0f), rt);

										XMVECTOR C = XMVector3TransformCoord(
											XMVectorSet(v2.x, v2.y, v2.z, 1.0f), rt);

										DX::DrawTriangle(m_batch.get(), A, B, C, Colors::LightGreen);
									}
								}
							}
							else
							{
								if (meshCol->GetMesh() == nullptr)
									break;

								const auto& meshes = meshCol->GetMesh()->meshData;

								// indexCount는 3의 배수여야 함(삼각형 리스트)
								for (size_t i = 0; i < meshes.vertices.size(); ++i)
								{
									auto& submesh = meshes.vertices[i];
									auto& submeshIndices = meshes.indices[i];

									const auto vertexCount = submesh.size();
									const size_t triCount = submeshIndices.size() / 3;
									for (size_t t = 0; t < triCount; ++t)
									{
										uint32_t ia = submeshIndices[t * 3 + 0];
										uint32_t ib = submeshIndices[t * 3 + 1];
										uint32_t ic = submeshIndices[t * 3 + 2];

										if (ia >= submesh.size() || ib >= vertexCount || ic >= vertexCount)
											continue; // 안전장치


										XMVECTOR A = XMVector3TransformCoord(XMLoadFloat3(&submesh[ia].Pos), rt);
										XMVECTOR B = XMVector3TransformCoord(XMLoadFloat3(&submesh[ib].Pos), rt);
										XMVECTOR C = XMVector3TransformCoord(XMLoadFloat3(&submesh[ic].Pos), rt);

										DX::DrawTriangle(m_batch.get(), A, B, C, Colors::LightGreen);
									}
								}
							}
						}
						break;
					}
					case ColliderComponent::DebugColliderType::Box:
					{
						BoundingBox box;
						box.Center = desc.localCenter;
						box.Extents = desc.halfExtents;
						BoundingOrientedBox obb;
						obb.CreateFromBoundingBox(obb, box);
						obb.Transform(obb, go->GetTransform()->GetWorldMatrix());
						DX::Draw(m_batch.get(), obb, Colors::LightGreen);
						break;
					}
					case ColliderComponent::DebugColliderType::Sphere:
					{
						BoundingSphere sphere;
						sphere.Center = desc.localCenter;
						sphere.Radius = desc.sphereRadius;
						DX::Draw(m_batch.get(), sphere, go->GetTransform()->GetWorldMatrix(), Colors::LightGreen);
						break;
					}
					case ColliderComponent::DebugColliderType::Capsule:
					{
						const auto& wm = go->GetTransform()->GetWorldMatrix();

						const Vector3 upV = wm.Up();
						const Vector3 rightV = wm.Right();
						const Vector3 forwardV = wm.Forward();

						const float r = desc.radius;

						const Vector3 worldPos = go->GetTransform()->GetWorldPosition() + desc.localCenter;
						const Vector3 p0 = worldPos + upV * desc.halfHeight; // 상단 구 중심
						const Vector3 p1 = worldPos - upV * desc.halfHeight; // 하단 구 중심

						const XMVECTOR color = Colors::LightGreen;

						// 축 벡터를 XMVECTOR로 (ring/arc에 사용)
						const XMVECTOR rightAxis = XMLoadFloat3(reinterpret_cast<const XMFLOAT3*>(&rightV)) * r;
						const XMVECTOR forwardAxis = XMLoadFloat3(reinterpret_cast<const XMFLOAT3*>(&forwardV)) * r;
						const XMVECTOR upAxis = XMLoadFloat3(reinterpret_cast<const XMFLOAT3*>(&upV)) * r;

						// ---- A) 절단원 링 2개 (원기둥 위/아래 단면) ----
						{
							const XMVECTOR o0 = XMLoadFloat3(reinterpret_cast<const XMFLOAT3*>(&p0));
							const XMVECTOR o1 = XMLoadFloat3(reinterpret_cast<const XMFLOAT3*>(&p1));

							// Up에 수직인 평면: major=Right*r, minor=Forward*r
							DX::DrawRing(m_batch.get(), o0, rightAxis, forwardAxis, color);
							DX::DrawRing(m_batch.get(), o1, rightAxis, forwardAxis, color);
						}

						// ---- B) 원기둥(측면) 4개 선 ----
						{
							m_batch->DrawLine(
								VertexPositionColor(p0 + rightV * r, Colors::LightGreen),
								VertexPositionColor(p1 + rightV * r, Colors::LightGreen));

							m_batch->DrawLine(
								VertexPositionColor(p0 - rightV * r, Colors::LightGreen),
								VertexPositionColor(p1 - rightV * r, Colors::LightGreen));

							m_batch->DrawLine(
								VertexPositionColor(p0 + forwardV * r, Colors::LightGreen),
								VertexPositionColor(p1 + forwardV * r, Colors::LightGreen));

							m_batch->DrawLine(
								VertexPositionColor(p0 - forwardV * r, Colors::LightGreen),
								VertexPositionColor(p1 - forwardV * r, Colors::LightGreen));
						}

						// ---- C) 상단 반구: 하프링 2개 ----
						{
							const XMVECTOR o = XMLoadFloat3(reinterpret_cast<const XMFLOAT3*>(&p0));

							// sin>=0 쪽이 +Up이므로 "상단" 반원
							DX::DrawHalfRing(m_batch.get(), o, rightAxis, upAxis, color, 16);
							DX::DrawHalfRing(m_batch.get(), o, forwardAxis, upAxis, color, 16);
						}

						// ---- D) 하단 반구: 하프링 2개 ----
						{
							const XMVECTOR o = XMLoadFloat3(reinterpret_cast<const XMFLOAT3*>(&p1));

							// 하단은 minorAxis를 -Up으로 뒤집으면 됨
							const XMVECTOR downAxis = -upAxis;

							DX::DrawHalfRing(m_batch.get(), o, rightAxis, downAxis, color, 16);
							DX::DrawHalfRing(m_batch.get(), o, forwardAxis, downAxis, color, 16);
						}

						break;
					}
					default:
						break;
					}
				}
			}
		}

		if (Camera::GetMainCamera().IsValid() &&
			g_selectedGameObject == Camera::GetMainCamera()->GetGameObject())
		{
			auto mainCam = Camera::GetMainCamera();

			if (mainCam.IsValid() && !mainCam->IsDestroyed())
			{
				BoundingFrustum frustum;
				BoundingFrustum::CreateFromMatrix(frustum, mainCam->GetProjMatrix());
				frustum.Transform(frustum, mainCam->GetTransform()->GetWorldMatrix());

				DX::Draw(m_batch.get(), frustum, Colors::GhostWhite);
			}
		}

			

		m_batch->End();
	}

	// Stencil 기반 마스크 생성 (선택된 오브젝트, 깊이 무시)
	if (g_selectedGameObject.IsValid() && !g_selectedGameObject->IsDestroyed()
		&& m_pPickingVS && m_pMaskPS && m_pStencilWriteState && m_pStencilTestState)
	{
		std::vector<uint32_t> selectedIds;
		auto renderers = g_selectedGameObject->GetComponents<Renderer>();
		selectedIds.reserve(renderers.size());

		for (auto& renderer : renderers)
		{
			if (!renderer.IsValid() || renderer->IsDestroyed())
				continue;

			uint32_t idx = renderer->GetRenderIndex();
			if (idx != UINT32_MAX)
				selectedIds.push_back(idx);
		}

		// 1) 스텐실 클리어
		context->ClearDepthStencilView(dsv, D3D11_CLEAR_STENCIL, 1.0f, 0);

		// 2) 선택 오브젝트를 스텐실에 기록 (컬러 쓰기 없음)
		context->OMSetRenderTargets(1, &rtv, dsv);
		context->OMSetDepthStencilState(m_pStencilWriteState.Get(), 1);
		context->OMSetBlendState(m_pNoColorWriteBS.Get(), nullptr, 0xFFFFFFFF);
		context->RSSetState(m_states->CullNone());

		if (!selectedIds.empty())
		{
			RenderManager::Get().RenderSelectedMask(
				m_pMaskPS->m_pPShader.Get(),
				selectedIds.data(),
				static_cast<uint32_t>(selectedIds.size()));
		}

		// 3) 스텐실 -> 마스크 텍스쳐로 변환
		context->OMSetRenderTargets(1, m_pMaskRTV.GetAddressOf(), dsv);
		context->OMSetDepthStencilState(m_pStencilTestState.Get(), 1);
		context->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);
		context->ClearRenderTargetView(m_pMaskRTV.Get(), DirectX::Colors::Black);

		context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		context->IASetInputLayout(nullptr);
		context->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);
		context->IASetIndexBuffer(nullptr, DXGI_FORMAT_R32_UINT, 0);
		context->VSSetShader(m_pFullScreenVS->m_pVShader.Get(), nullptr, 0);
		context->PSSetShader(m_pMaskPS->m_pPShader.Get(), nullptr, 0);

		context->Draw(3, 0);

		context->OMSetDepthStencilState(nullptr, 0);
	}

	// 아웃라인 렌더링 (씬 뷰 전용)
	if (g_selectedGameObject.IsValid() && !g_selectedGameObject->IsDestroyed()
		&& m_pOutlinePS && m_pFullScreenVS && m_pOutlineCBuffer && m_pMaskSRV)
	{
		if (m_width > 0 && m_height > 0)
		{
			OutlineConstants constants;
			constants.color = { 1.0f, 0.5f, 0.0f, 1.0f };
			constants.texelSize = { 1.0f / static_cast<float>(m_width), 1.0f / static_cast<float>(m_height) };
			constants.thickness = 1.0f;
			constants.threshold = 0.1f;

			context->UpdateSubresource(m_pOutlineCBuffer.Get(), 0, nullptr, &constants, 0, 0);

			context->OMSetRenderTargets(1, &rtv, dsv);
			context->OMSetBlendState(m_pOutlineBlendState.Get(), nullptr, 0xffffffff);
			context->OMSetDepthStencilState(m_states->DepthNone(), 0);
			context->RSSetState(m_states->CullNone());

			context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			context->IASetInputLayout(nullptr);
			context->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);
			context->IASetIndexBuffer(nullptr, DXGI_FORMAT_R32_UINT, 0);
			context->VSSetShader(m_pFullScreenVS->m_pVShader.Get(), nullptr, 0);
			context->PSSetShader(m_pOutlinePS->m_pPShader.Get(), nullptr, 0);

			ID3D11ShaderResourceView* maskSrv = m_pMaskSRV.Get();
			context->PSSetShaderResources(0, 1, &maskSrv);
			context->PSSetConstantBuffers(0, 1, m_pOutlineCBuffer.GetAddressOf());

			context->Draw(3, 0);

			ID3D11ShaderResourceView* nullSRV2 = nullptr;
			context->PSSetShaderResources(0, 1, &nullSRV2);
		}
	}

	if (m_ui2DMode)
		RenderManager::Get().RenderUIWithSize(static_cast<UINT>(m_width), static_cast<UINT>(m_height));

	// 여기서 함수 끝나면 guard 소멸자에서 원래 RT/Viewport/Blend 등 자동 복원됨
}
