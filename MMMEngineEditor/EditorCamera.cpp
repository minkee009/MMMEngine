#include "EditorCamera.h"
#include "MMMInput.h"
#include "MMMTime.h"

using namespace DirectX::SimpleMath;
using namespace DirectX;

struct FocusState {
    bool active = false;
    Vector3 targetPos;
    float duration = 0.5f; // 이동 시간
    float elapsedTime = 0.0f;
    Vector3 startPos;
};

FocusState m_focusState;

namespace CameraMathf
{
    float Lerp(float a, float b, float t)
    {
        return (a + (b - a) * t);
    }
}

void MMMEngine::Editor::EditorCamera::FocusOn(const Vector3& worldPosition, float distance)
{
    m_focusState.startPos = GetPosition();
    
    // 1. 현재 카메라가 바라보고 있는 방향 벡터를 가져옵니다.
    Matrix worldMat = GetTransformMatrix();
    Vector3 lookDir = -worldMat.Forward(); // 카메라가 보고 있는 앞방향
    
    // 2. 목표 위치 결정
    // 물체 위치(worldPosition)에서 바라보는 방향의 반대(-lookDir)로 distance만큼 이동
    m_focusState.targetPos = worldPosition - (lookDir * distance);

    m_focusState.elapsedTime = 0.0f;
    m_focusState.active = true;
}

void MMMEngine::Editor::EditorCamera::UpdateProjMatrix()
{
    const float orthoHeight = m_orthoSize;
    const float orthoWidth = orthoHeight * m_aspect;
    const float fovRadians = DirectX::XMConvertToRadians(m_fov);

    Matrix persp;
    Matrix ortho;
    XMStoreFloat4x4(&persp, XMMatrixPerspectiveFovLH(fovRadians, m_aspect, m_near, m_far));
    XMStoreFloat4x4(&ortho, XMMatrixOrthographicLH(orthoWidth, orthoHeight, m_near, m_far));

    if (m_projectionBlend <= 0.0f)
    {
        m_cachedProjMatrix = persp;
    }
    else if (m_projectionBlend >= 1.0f)
    {
        m_cachedProjMatrix = ortho;
    }
    else
    {
        m_cachedProjMatrix = Matrix::Lerp(persp, ortho, m_projectionBlend);
    }
}

void MMMEngine::Editor::EditorCamera::UpdateProjFrustum()
{
    BoundingFrustum::CreateFromMatrix(m_cachedProjFrustum, m_cachedProjMatrix);
}

const DirectX::SimpleMath::Matrix MMMEngine::Editor::EditorCamera::GetCameraMatrix()
{
    return GetViewMatrix() * GetProjMatrix();
}

const DirectX::SimpleMath::Matrix& MMMEngine::Editor::EditorCamera::GetViewMatrix()
{
    if (m_isViewMatrixDirty)
    {
		m_cachedViewMatrix = GetTransformMatrix().Invert();
        m_isViewMatrixDirty = false;
    }

    return m_cachedViewMatrix;
}

const DirectX::SimpleMath::Matrix& MMMEngine::Editor::EditorCamera::GetProjMatrix()
{
    if (m_isProjMatrixDirty)
    {
        UpdateProjMatrix();
        UpdateProjFrustum();
        m_isProjMatrixDirty = false;
    }

    return m_cachedProjMatrix;
}

const DirectX::BoundingFrustum& MMMEngine::Editor::EditorCamera::GetProjFrustum()
{
    if (m_isProjMatrixDirty)
    {
        UpdateProjMatrix();
        UpdateProjFrustum();
        m_isProjMatrixDirty = false;
    }

    return m_cachedProjFrustum;
}

void MMMEngine::Editor::EditorCamera::MarkViewMatrixDirty()
{
    m_isViewMatrixDirty = true;
}

void MMMEngine::Editor::EditorCamera::MarkProjectionMatrixDirty()
{
    m_isProjMatrixDirty = true;
}

void MMMEngine::Editor::EditorCamera::MarkTransformMatrixDirty()
{
    m_isTransformMatrixDirty = true;
    m_isViewMatrixDirty = true;
}

void MMMEngine::Editor::EditorCamera::UpdateProjectionBlend()
{
    float target = m_isOrthographic ? 1.0f : 0.0f;
    float delta = target - m_projectionBlend;

    if (delta > 0.0001f || delta < -0.0001f)
    {
        float step = m_projectionBlendSpeed * Time::GetUnscaledDeltaTime();
        if (m_projectionBlend < target)
        {
            m_projectionBlend += step;
            if (m_projectionBlend > target)
                m_projectionBlend = target;
        }
        else
        {
            m_projectionBlend -= step;
            if (m_projectionBlend < target)
                m_projectionBlend = target;
        }
        MarkProjectionMatrixDirty();
    }
    else if (m_projectionBlend != target)
    {
        m_projectionBlend = target;
        MarkProjectionMatrixDirty();
    }
}

void MMMEngine::Editor::EditorCamera::UpdateState()
{
    const float moveSpeed = 5.0f;

    if (m_inputStateDirty)
    {
        auto euler = m_rotation.ToEuler();
        m_targetPitch = m_pitch = DirectX::XMConvertToDegrees(euler.x);
        m_targetYaw = m_yaw = DirectX::XMConvertToDegrees(euler.y);
        m_smoothedMovement = Vector3::Zero;
        m_targetMovement = Vector3::Zero;
        m_inputStateDirty = false;
    }

    if (m_focusState.active)
    {
        m_focusState.elapsedTime += Time::GetUnscaledDeltaTime();
        float t = m_focusState.elapsedTime / m_focusState.duration;

        if (t >= 1.0f)
        {
            SetPosition(m_focusState.targetPos);
            m_focusState.active = false;

            // [중요] 포커스 완료 후 모든 보간 변수를 현재의 물리적 상태로 덮어쓰기
            auto euler = m_rotation.ToEuler();
            m_targetPitch = m_pitch = DirectX::XMConvertToDegrees(euler.x);
            m_targetYaw = m_yaw = DirectX::XMConvertToDegrees(euler.y);
            m_smoothedMovement = Vector3::Zero;
            m_targetMovement = Vector3::Zero;
        }
        else
        {
            t = t * t * (3.0f - 2.0f * t); // SmoothStep
            SetPosition(Vector3::Lerp(m_focusState.startPos, m_focusState.targetPos, t));

            // [중요] 포커스 도중에도 마우스 좌표는 계속 업데이트해서 delta 튐 방지
            auto mousePos = Input::GetMousePos();
            m_lastMouseX = mousePos.x;
            m_lastMouseY = mousePos.y;
            m_hasInput = false;
            return;
        }
    }

    if (!m_hasInput)
    {
        m_targetMovement = Vector3::Zero;
    }

    m_pitch = CameraMathf::Lerp(m_pitch, m_targetPitch, 12.0f * Time::GetUnscaledDeltaTime());
    m_yaw = CameraMathf::Lerp(m_yaw, m_targetYaw, 12.0f * Time::GetUnscaledDeltaTime());
    SetEulerRotation(Vector3(m_pitch, m_yaw, 0));

    m_smoothedMovement = Vector3::Lerp(m_smoothedMovement, m_targetMovement, 6.0f * Time::GetUnscaledDeltaTime());
    SetPosition(GetPosition() + m_smoothedMovement * moveSpeed * Time::GetUnscaledDeltaTime());

    m_hasInput = false;
}

void MMMEngine::Editor::EditorCamera::InputUpdate(int currentOp, bool isHovered)
{
    const float rotSpeed = 0.1f;
    const float panSpeed = 0.01f; // 팬 이동 속도

    // 포커스 이동 애니메이션 중일 땐 마우스 좌표만 갱신
    if (m_focusState.active)
    {
        auto mousePos = Input::GetMousePos();
        m_lastMouseX = mousePos.x;
        m_lastMouseY = mousePos.y;
        return;
    }

    auto mousePos = Input::GetMousePos();
    float deltaX = mousePos.x - m_lastMouseX;
    float deltaY = mousePos.y - m_lastMouseY;

    // 현재 사용자가 누르고 있는 마우스 버튼 상태 확인 (휠 클릭은 엔진에 맞춰 KeyCode::MouseMiddle 등을 사용하세요)
    bool isRightClick = Input::GetKey(KeyCode::MouseRight);
    bool isMiddleClick = Input::GetKey(KeyCode::MouseMiddle);
    bool isLeftClick = (currentOp == 0 && Input::GetKey(KeyCode::MouseLeft));

    // 아무 버튼도 눌려있지 않으면 드래그 상태 해제
    if (!isRightClick && !isMiddleClick && !isLeftClick)
    {
        m_isDragging = false;
        if (Input::GetKeyUp(KeyCode::MouseRight)) m_firstMouseUpdate = true;
    }

    // ★ 핵심 조건: 마우스가 SceneView 위에 있거나, 이미 SceneView 안에서 드래그를 시작한 상태일 때만 입력을 처리
    bool canProcessInput = isHovered || m_isDragging;
    float wheel = Input::GetMouseScrollNotches();

    // 조작 불가능한 상태라면 마우스 좌표만 갱신하고 빠져나감
    if (!canProcessInput)
    {
        m_hasInput = false;
        m_targetMovement = Vector3::Zero;
        m_lastMouseX = mousePos.x;
        m_lastMouseY = mousePos.y;
        return;
    }

    // 허용된 상태에서 버튼이 눌려있다면 드래그 중인 상태로 전환
    if (isRightClick || isMiddleClick || isLeftClick)
    {
        m_isDragging = true;
    }

    m_hasInput = true;
    m_targetMovement = Vector3::Zero;

    // --- 실제 카메라 이동 로직 ---
    if (isRightClick)
    {
        // 마우스 우클릭 회전 및 WASD 이동 로직 (기존과 동일)
        if (!m_firstMouseUpdate)
        {
            if (deltaX != 0 || deltaY != 0)
            {
                m_targetYaw += deltaX * rotSpeed;
                m_targetPitch += deltaY * rotSpeed;
            }
        }
        else
        {
            m_firstMouseUpdate = false;
        }

        Matrix worldMat = GetTransformMatrix();
        Vector3 move =
        {
            (Input::GetKey(KeyCode::A) ? -1.0f : 0.0f) + (Input::GetKey(KeyCode::D) ? 1.0f : 0.0f),
            (Input::GetKey(KeyCode::Q) ? -1.0f : 0.0f) + (Input::GetKey(KeyCode::E) ? 1.0f : 0.0f),
            (Input::GetKey(KeyCode::S) ? -1.0f : 0.0f) + (Input::GetKey(KeyCode::W) ? 1.0f : 0.0f)
        };

        m_targetMovement += worldMat.Backward() * move.z;
        m_targetMovement += worldMat.Right() * move.x;
        m_targetMovement += worldMat.Up() * move.y;
    }
    // 휠 스크롤(줌): 마우스가 화면 위에 올라가 있을 때(Hovered)만 적용
    else if (wheel != 0.0f && isHovered)
    {
        m_smoothedMovement = Vector3::Zero;
        m_targetMovement = Vector3::Zero;

        if (m_isOrthographic)
        {
            const float zoomSpeed = 1.0f;
            SetOrthoSize(m_orthoSize - (wheel * zoomSpeed));
        }
        else
        {
            Matrix worldMat = GetTransformMatrix();
            Vector3 forward = worldMat.Backward();
            const float zoomSpeed = 2.0f;
            SetPosition(GetPosition() + forward * (wheel * zoomSpeed));
        }
    }
    // 좌클릭(currentOp == 0) 이거나 휠 드래그(가운데 버튼)일 때 패닝
    else if (isLeftClick || isMiddleClick)
    {
        Matrix worldMat = GetTransformMatrix();
        m_smoothedMovement = Vector3::Zero; // 핸드 조작 시 관성 제거
        Vector3 deltaPos = (worldMat.Right() * (-deltaX * panSpeed)) + (worldMat.Up() * (deltaY * panSpeed));
        SetPosition(GetPosition() + deltaPos);
    }

    if (m_targetMovement.LengthSquared() > 0.0f) m_targetMovement.Normalize();
    if (Input::GetKey(KeyCode::LeftShift)) m_targetMovement *= 3.0f;

    // 마우스 위치 업데이트
    m_lastMouseX = mousePos.x;
    m_lastMouseY = mousePos.y;
}
