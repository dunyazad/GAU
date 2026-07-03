# CLAUDE.md — Blueprint Style Node Editor (내부용)

## 1. 프로젝트 개요

언리얼 엔진 블루프린트의 디자인과 기본 동작을 그대로 따르는 크로스플랫폼
노드 그래프 에디터를 제작한다. 내부 사용 목적이며 상용 배포는 없다.

- 대상 플랫폼: Windows, macOS, iOS (Linux는 빌드 유지 수준)
- 언어: C++17
- 윈도우/입력: SDL3
- 렌더링: NanoVG
  - Windows/Linux: nanovg_gl (OpenGL 3.3 Core)
  - macOS/iOS: MetalNanoVG (ollix/MetalNanoVG)
- 빌드: CMake (데스크톱), Xcode 프로젝트 (iOS)
- JSON: nlohmann/json (external/nlohmann/json.hpp 단일 헤더 벤더링)
- 외부 의존성은 위 목록 외 추가 금지. 필요 시 먼저 사용자에게 확인할 것.

## 2. 코딩 컨벤션 (엄수)

1. 주석은 ASCII 문자만 사용한다. 한글/유니코드 주석 금지.
2. 코드는 항상 완전한 함수 단위로 작성한다.
   - 파일 전체 또는 완전한 함수 본문만 제출한다.
   - "N번째 줄에 삽입" 같은 부분 패치 지시는 금지.
3. 긴 함수는 이름 있는 헬퍼 함수로 분리하고, 각 헬퍼도 완전한 본문으로 작성한다.
4. 문제 수정은 근본 원인 해결만 허용한다. 임시방편/우회 패치 금지.
   - 원인 분석 결과를 먼저 서술한 뒤 수정한다.
5. 답변과 설명은 한국어(존댓말)로 한다. 코드 식별자와 주석은 영어(ASCII).
6. 네이밍:
   - 타입/함수: PascalCase (예: NodeGraph, DrawNodeBody)
   - 변수/멤버: camelCase (예: panOffset, hoveredPinId)
   - 상수/매크로: UPPER_SNAKE_CASE
7. 예외 사용 금지. 실패는 반환값(bool/enum/nullptr)으로 전달한다.
8. 소유권은 명시적으로: 컨테이너는 std::vector, 소유 포인터는
   std::unique_ptr, 참조 전달은 raw pointer 또는 참조.
9. 플랫폼 분기(#if defined(__APPLE__) 등)는 src/platform/ 내부에만 존재
   해야 한다. 다른 계층에서 플랫폼 매크로 사용 금지.

## 3. 디렉터리 구조

```
/
├── CLAUDE.md
├── CMakeLists.txt
├── external/            # SDL3, nanovg, MetalNanoVG (서브모듈 또는 벤더링)
├── src/
│   ├── platform/        # 유일하게 플랫폼 분기가 허용되는 계층
│   │   ├── PlatformWindow.h/.cpp     # SDL 윈도우 생성, 프레임 begin/end
│   │   ├── PlatformNVG.h/.cpp/.mm    # NVG 컨텍스트 생성 (GL vs Metal)
│   │   └── PlatformInput.h/.cpp      # SDL 이벤트 -> EditorInputEvent 변환
│   ├── render/          # NanoVG 기반 그리기. 상태 없음(stateless) 함수 집합
│   │   ├── Canvas.h/.cpp             # 좌표 변환, 팬/줌 상태
│   │   ├── GridRenderer.h/.cpp
│   │   ├── NodeRenderer.h/.cpp       # 노드 본체, 헤더, 핀 아이콘
│   │   └── LinkRenderer.h/.cpp       # 베지어 링크, 드래그 중 임시 링크
│   ├── model/           # 순수 데이터. 렌더링/SDL 의존 금지
│   │   ├── GraphTypes.h              # NodeId, PinId, LinkId, PinType 등
│   │   ├── NodeGraph.h/.cpp          # 노드/핀/링크 컨테이너, 연결 규칙
│   │   ├── GraphSerializer.h/.cpp    # JSON 저장/로드
│   │   └── UndoStack.h/.cpp          # 커맨드 패턴 언두/리두
│   ├── interaction/     # 입력 상태 머신. 렌더링 의존 금지, 모델만 조작
│   │   ├── EditorState.h             # 상태 enum, 캐퍼빌리티 플래그
│   │   ├── HitTest.h/.cpp            # 노드/핀/링크 히트 테스트
│   │   └── InteractionFSM.h/.cpp     # 상태 전이 처리
│   ├── exec/            # 그래프 실행 엔진
│   │   ├── ExecEngine.h/.cpp         # exec 핀 순차 실행, data 핀 pull 평가
│   │   └── BuiltinNodes.h/.cpp       # 기본 노드 정의 (Branch, Print 등)
│   └── main.cpp
└── ios/                 # Xcode 프로젝트 (마일스톤 M6에서 생성)
```

## 4. 아키텍처 원칙

의존 방향은 단방향으로 강제한다:

```
platform -> (없음, SDL/OS만)
render   -> model (읽기 전용)
interaction -> model (쓰기), HitTest는 render의 레이아웃 결과를 입력으로 받음
exec     -> model (읽기 전용)
main.cpp -> 전 계층 조립
```

- model 계층은 SDL, NanoVG, OpenGL, Metal 헤더를 include하지 않는다.
- 노드의 화면 레이아웃(핀 위치 등)은 render 단계에서 계산하여
  프레임 캐시(NodeLayoutCache)에 기록하고, HitTest는 이 캐시만 읽는다.
- 입력 이벤트는 PlatformInput이 EditorInputEvent로 정규화한 뒤
  InteractionFSM에 전달한다. FSM 외부에서 마우스 상태 직접 조회 금지.

## 5. 블루프린트 디자인 스펙

다음 수치를 기준값으로 사용한다. 모두 캔버스 단위(줌 1.0 기준)이다.

- 그리드: 소격자 16px, 대격자 = 소격자 x 8
  - 배경색 rgb(30,30,33), 소격자 rgb(41,41,43), 대격자 rgb(26,26,28)
- 노드: 모서리 반경 6px, 최소 폭 160px
  - 본체 rgba(24,24,28,235), 테두리 rgb(60,60,66)
  - 선택 테두리 rgb(255,180,40), 두께 2px
  - 헤더 높이 26px, 좌->우 수평 그라데이션 (기본색 -> 기본색 x 0.4)
  - 헤더 색상 팔레트:
    - Event: rgb(150,30,30), Function: rgb(40,80,160)
    - FlowControl: rgb(90,90,100), Pure: rgb(60,120,60)
- 핀:
  - exec 핀: 오각형 화살표, 흰색, 연결 시 채움 / 미연결 시 외곽선
  - data 핀: 원형 반경 5px, 타입별 색
    - bool rgb(140,0,0), int rgb(30,200,160), float rgb(160,250,60)
    - string rgb(250,0,220), object rgb(0,160,240)
  - 히트 반경: 데스크톱 10px, 터치 환경 24px
- 링크: 3차 베지어, 수평 접선
  - tangent = |dx| < 200 ? |dx| * 0.5 + 50 : |dx| * 0.5
  - exec 링크 흰색 3px, data 링크 핀 색상 2px
- 줌 범위: 0.1 ~ 4.0, 휠/핀치 모두 커서(제스처 중심) 고정 줌

## 6. 인터랙션 상태 머신 스펙

상태 목록:

```
Idle
PanningCanvas        (우클릭 드래그 또는 스페이스+드래그, 터치는 두 손가락)
DraggingNodes        (선택된 노드 집합 이동)
RubberBandSelect     (빈 캔버스에서 좌클릭 드래그)
DraggingLink         (핀에서 드래그, 호환 핀 위에서 놓으면 링크 생성)
CuttingLinks         (Ctrl+드래그로 절단선, 교차 링크 삭제 -- M4)
ContextMenuOpen      (우클릭 짧게 / 터치 롱프레스)
```

규칙:

- 히트 우선순위: 핀 > 노드 > 링크 > 캔버스
- 링크 생성 검증은 model의 NodeGraph::CanConnect가 단일 진실 공급원이다.
  - 같은 노드 내 연결 금지, 타입 불일치 금지, exec-data 혼합 금지,
    입력 핀은 단일 연결(기존 링크 교체), 순환은 exec 그래프에서만 금지
- 터치 환경은 hasHover=false 캐퍼빌리티로 처리:
  - 호버 하이라이트 없음, 핀 히트 반경 24px, 롱프레스 500ms = 컨텍스트 메뉴
- 모델을 변경하는 모든 조작(노드 이동 완료, 링크 생성/삭제, 노드 추가/삭제)은
  UndoStack 커맨드로만 수행한다. 직접 변경 금지.

## 7. 실행 엔진 스펙

언리얼과 동일한 이원 구조:

- exec 핀: 흰색 실행 흐름. ExecEngine이 링크를 따라 순차 실행.
- data 핀: pull 방식. 노드 실행 직전에 입력 data 핀을 재귀 평가.
  - Pure 노드(헤더 없는 연산 노드)는 요청 시마다 평가.
- Value는 std::variant<bool, int, double, std::string> 기반.
- 무한 루프 방지: 프레임당 노드 실행 상한(기본 10000) 초과 시 중단하고
  마지막 실행 노드 ID를 포함한 오류를 보고한다.
- 기본 제공 노드(M5): EventBegin, Branch, Sequence, PrintString,
  MakeInt, MakeFloat, Add, Compare, ForLoop

## 8. 마일스톤

각 마일스톤은 독립적으로 빌드/실행 가능해야 하며, 완료 조건(AC)을
모두 만족한 뒤 다음으로 진행한다. 한 세션에 여러 마일스톤을 진행하지
말 것.

### M0. 빌드 골격
- CMake + SDL3 + nanovg_gl로 빈 창 표시 (Windows 기준)
- AC: 창 표시, 리사이즈 동작, 종료 시 리소스 정리, 콘솔 에러 없음

### M1. 캔버스
- Canvas(팬/줌), GridRenderer, 좌표 변환 단위 테스트
- AC: 휠 줌이 커서 고정으로 동작, 우클릭 드래그 팬, 그리드 2단 렌더링

### M2. 노드 렌더링
- model의 NodeGraph 최소 구현(노드/핀 정의), NodeRenderer, 레이아웃 캐시
- AC: 디자인 스펙 5절의 색상/수치대로 노드 3종(Event/Function/Pure)이
  하드코딩 그래프로 표시됨

### M3. 인터랙션 1차
- HitTest, InteractionFSM: Idle/Panning/DraggingNodes/RubberBandSelect
- AC: 노드 드래그, 다중 선택, 선택 테두리 표시, 팬과 드래그 충돌 없음

### M4. 링크
- LinkRenderer, DraggingLink 상태, CanConnect 규칙, CuttingLinks
- AC: 핀 드래그로 링크 생성, 비호환 핀에 스냅 안 됨, 입력 핀 교체 동작,
  절단선으로 삭제, 모든 변경이 언두/리두 가능

### M5. 실행 엔진 + 직렬화
- ExecEngine, BuiltinNodes, GraphSerializer(JSON), 노드 검색 팔레트
- AC: EventBegin -> Branch -> PrintString 그래프가 실행되어 로그 출력,
  저장 후 재시작하여 로드 시 동일 그래프 복원

### M6. Apple 플랫폼
- PlatformNVG의 MetalNanoVG 경로(.mm), macOS 빌드, iOS Xcode 프로젝트
- 터치 입력: 핀치 줌, 두 손가락 팬, 롱프레스 메뉴, 확대 핀 히트 반경
- AC: 동일 그래프 파일이 3개 플랫폼에서 동일하게 표시/편집됨

## 9. 작업 방식

- 각 작업 시작 시 해당 마일스톤의 AC를 먼저 확인하고 계획을 요약한다.
- 파일을 수정할 때는 수정되는 함수의 완전한 본문을 출력한다.
- 버그 보고에서 "터졌다"는 런타임 크래시를 의미한다(컴파일 오류 아님).
  크래시 보고를 받으면 재현 경로와 근본 원인 분석을 먼저 수행한다.
- 렌더링 코드와 모델 코드가 한 함수에 섞이면 리팩터링을 우선한다.
- 커밋 메시지는 영어, 형식: `M{n}: {summary}` (예: `M2: add node header gradient`)
- 