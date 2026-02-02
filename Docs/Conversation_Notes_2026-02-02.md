# MMMEngine 작업 메모 (2026-02-02)

이 파일은 Cursor 대화 내용을 다른 환경에서 이어서 진행하기 위한 요약입니다.

## 1) 완료된 변경 사항

- `USCRIPT_PROPERTY_HIDDEN()` 의미 변경
  - RTTR 등록은 유지
  - 인스펙터에서만 숨김 (`rttr::metadata("INSPECTOR","HIDDEN")`)
- 생성기 변경
  - `MMMEngineEditor/UserScriptsGenerator.cpp`에서
    `USCRIPT_PROPERTY_HIDDEN()`을 파싱해 `inspectorHidden=true`로 기록
  - gen.cpp 생성 시 해당 프로퍼티에
    `(rttr::metadata("INSPECTOR","HIDDEN"))` 추가
- 문서 업데이트
  - `Docs/UserScripts_작성법.md`의 2.4 항목 수정
  - 인스펙터/저장 모두 불필요하면 매크로 미사용 권장 문장 추가

## 2) ObjPtr<T> 프로퍼티 + 전방선언 이슈

- `ObjPtr<T>` 프로퍼티가 RTTR `.property(...)`로 등록될 때
  `T`는 완전한 타입이어야 함.
- 전방선언만 있으면 `UserScripts.gen.cpp`에서 컴파일 에러 발생.
- 해결 핵심은 gen.cpp에 필요한 헤더를 추가 include 하는 것.

## 3) 후속 설계 방향 (제안)

- 엔진 타입 매핑 테이블
  - 프로젝트 생성 시 고정 텍스트/JSON로 생성
  - 예: `Transform -> MMMEngineShared/Transform.h`
- 유저스크립트 타입 매핑
  - `Source/UserScripts/Scripts/` 스캔으로 자동 갱신
- gen.cpp 생성 시 `ObjPtr<T>`에 대해 매핑 테이블을 조회하여
  필요한 헤더 include 추가

## 4) 관련 코드 위치

- 생성기: `MMMEngineEditor/UserScriptsGenerator.cpp`
- 인스펙터 숨김 처리:
  `MMMEngineEditor/InspectorWindow.cpp`
  (`INSPECTOR` 메타데이터 사용)
- 문서: `Docs/UserScripts_작성법.md`
