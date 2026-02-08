# UI 좌표 변환 헬퍼

이 문서는 Canvas/RectTransform 좌표 변환을 단순하게 쓰기 위한 헬퍼 API를 설명합니다.

**좌표 공간 요약**
- Screen(Scene): RenderManager의 SceneSize 기준 픽셀 좌표. 원점은 좌상단.
- Canvas: UI 레이아웃 좌표. ScaleWithScreenSize일 때 Screen과 다를 수 있음.

**신규 API**
```cpp
DirectX::SimpleMath::Vector2 Canvas::ScreenToCanvas(const Vector2& screenPos) const;
DirectX::SimpleMath::Vector2 RectTransform::GetPivotPositionInCanvas(const Vector2& canvasSize) const;
DirectX::SimpleMath::Vector2 RectTransform::GetAnchoredPositionInCanvas(const Vector2& canvasSize) const;
```

**사용 예시 1: 월드 좌표 -> 스크린 -> 캔버스**
```cpp
auto cam = Camera::GetMainCamera();
auto canvas = graphic->GetCanvas();
Vector3 screenPos = cam->WorldToScreenPoint(worldPos);
Vector2 canvasPos = canvas->ScreenToCanvas(Vector2(screenPos.x, screenPos.y));
```

**사용 예시 2: UI 피벗 위치(캔버스 좌표)**
```cpp
auto rectTr = GetTransform().Cast<RectTransform>();
Vector2 canvasSize = canvas->GetCanvasSize();
Vector2 uiPivotCanvas = rectTr->GetPivotPositionInCanvas(canvasSize);
```

**주의 사항**
- `RectTransform::GetAnchoredPosition()`은 로컬 좌표이며 캔버스 좌표가 아닙니다.
- `ScaleWithScreenSize`에서는 반드시 `ScreenToCanvas()`를 거쳐야 정확한 캔버스 좌표가 됩니다.
