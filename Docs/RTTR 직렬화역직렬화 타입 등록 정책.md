# RTTR 직렬화/역직렬화 타입 등록 정책

## 개요

RTTR에서 `.constructor<>()`만 등록하고 정책을 지정하지 않으면 **기본값은 `as_std_shared_ptr`** 입니다.  
이 경우 `type::create()`가 **값이 아니라 `std::shared_ptr<T>`** 를 반환하므로, 일반 직렬화/역직렬화 경로에서 컨테이너 요소 타입은 **`as_object`** 로 등록해야 프로퍼티가 올바르게 채워집니다.

- **문서**: `registration.h` – "The default constructor create policy is: policy::ctor::as_std_shared_ptr"
- **구현**: `rttr/detail/registration/bind_impl.h` – `using default_create_policy = detail::as_std_shared_ptr;`

---

## 확인된 타입 정리

| 타입                | 등록 위치             | constructor | 정책                      | 컨테이너 사용                | 결론                                                         |
| ------------------- | --------------------- | ----------- | ------------------------- | ---------------------------- | ------------------------------------------------------------ |
| **CurveKeyframe**   | AnimationCurve.cpp    | ✅           | **as_object**             | `AnimationCurve::keyframes`  | 이미 수정됨 ✅                                                |
| **AnimationCurve**  | AnimationCurve.cpp    | ✅           | **as_object**             | (자체가 프로퍼티)            | 이미 수정됨 ✅                                                |
| **PersistentCall**  | SerializableEvent.cpp | ❌ 없음      | -                         | `SerializableEvent::Calls`   | 하드코딩 분기로 역직렬화 → `type::create()` 미사용. as_object 불필요. |
| **Mesh_Vertex**     | RenderShared.cpp      | ✅           | 없음(→ as_std_shared_ptr) | `MeshData::vertices`         | 일반 경로로 로드하면 **as_object 필요**                      |
| **Mesh_BoneBuffer** | RenderShared.cpp      | ✅           | 없음                      | (메시 데이터 내부)           | 일반 경로 사용 시 **as_object 필요**                         |
| **Mesh_AnimTrack**  | RenderShared.cpp      | ✅           | 없음                      | posKeys/rotKeys/scaleKeys 등 | 일반 경로 사용 시 **as_object 필요**                         |
| **Mesh_VecKey**     | RenderShared.cpp      | ✅           | 없음                      | Mesh_AnimTrack 내부          | 일반 경로 사용 시 **as_object 필요**                         |
| **Mesh_QuatKey**    | RenderShared.cpp      | ✅           | 없음                      | Mesh_AnimTrack 내부          | 일반 경로 사용 시 **as_object 필요**                         |

---

## 정책 요약

- **컨테이너 요소로 쓰이면서** `DeserializeVariant` → `is_sequential_container()` → `value_type.create()` → `DeserializeObject(inst, j)` **일반 경로**를 타는 타입  
  → **`.constructor<>()(rttr::policy::ctor::as_object)`** 로 등록하는 것이 올바름.
- **하드코딩 분기**로만 직렬화/역직렬화하는 타입(예: SerializableEvent, PersistentCall)은 `type::create()`를 쓰지 않으므로 as_object 여부와 무관.
- **기본(as_std_shared_ptr)** 만 쓰면 `type::create()`가 `shared_ptr<T>`를 반환해, `DeserializeObject`에서 프로퍼티가 비어 역직렬화가 올바르게 되지 않음.