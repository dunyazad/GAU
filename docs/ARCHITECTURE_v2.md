# GAU v2 아키텍처 설계

- 상태: 그린필드 재구조 (v1은 `v1` 브랜치로 보존, v2는 `main`).
- 방식: 새 코어를 라이브러리로 additive하게 세우고 검증된 v1 코드를 이식/대체한다.
- UI: 리테인드(retained) UI 도입 (보존 위젯 트리 + 레이아웃).

---

## 1. 목표와 원칙

### 1.1 v2가 요구하는 것 (SRS 근거)
- struct/컨테이너 값이 실행 흐름을 따라 전달 (exec struct flow).
- 컨테이너의 사용자 타입 (`Array<Vector3f>`), Make/Break 자동 노드.
- 서브그래프/함수 노드, 프로젝트 단위 저장.
- 실행 디버깅 (스텝/브레이크포인트/watch) — 중단 가능한 런타임.
- 다수 패널 UI (미니맵/디버거/라이브러리/프로퍼티).

### 1.2 v1 구조의 한계
- `main.cpp` 비대: 조립 + 이벤트 루프 + 글루 헬퍼가 한 파일(~1900줄).
- `Value`가 단순 `variant<bool,int,double,string>` — struct/컨테이너 흐름 부담.
- 그래프가 단일·평면 — 서브그래프/프로젝트 개념 없음.
- 실행 엔진이 단발 `Run()` — 중단/스텝 불가.
- 즉시모드 UI가 위젯마다 rect 수식 하드코딩 — 패널 증가 시 폭증.

### 1.3 원칙 (v1 규약 계승)
- C++17, 예외 금지(반환값 실패 전달), ASCII 주석.
- 의존 방향 단방향. `core`/`model`은 SDL/렌더 미포함.
- 플랫폼 분기는 `platform/`에만.
- 외부 의존성: SDL3, NanoVG, nlohmann/json, wasm3 (추가 시 사전 승인).

---

## 2. 계층 구조

```
core        -> (없음)                 순수 타입/값/유틸
model       -> core                   그래프/노드/프로젝트/커맨드
exec        -> model, core            런타임 VM + 디버그
render      -> model, core (읽기)     NanoVG stateless 그리기
ui          -> render, core           리테인드 위젯 트리 + 레이아웃
interaction -> model, ui              입력 -> 위젯/그래프 조작
app         -> 전 계층                Application/Project/Document 조립
platform    -> SDL/OS만
```

의존은 위에서 아래로만 흐른다. `app`이 유일한 조립 지점이며, `main.cpp`는 얇은 진입점으로 축소한다.

### 2.1 디렉터리
```
src/
  core/         # Type, TypeId, TypeRegistry, Value, Result
  model/        # Node, Pin, Link, Graph, Subgraph, Project, Command/Undo, Serialize
  exec/         # Runtime(VM), DebugSession, BuiltinNodes
  render/       # Canvas, GridRenderer, NodeRenderer, LinkRenderer (stateless)
  ui/           # Widget, Layout, Panel, Button, Dropdown, TextField, event dispatch
  interaction/  # InteractionFSM, HitTest, controllers
  app/          # Application, Document, InputRouter, wiring
  platform/     # PlatformWindow, PlatformNVG, PlatformInput (SDL/OS)
  main.cpp      # thin entry
tests/          # core_tests, model_tests, exec_tests, ...
external/       # SDL, nanovg, nlohmann, wasm3
```

### 2.2 빌드 전략
- 각 계층을 정적 라이브러리로: `gau_core`, `gau_model`, `gau_exec`, `gau_render`, `gau_ui`, `gau_interaction`, `gau_platform`.
- `gau` 실행 파일은 `app` + 라이브러리 링크.
- 계층별 단위 테스트 타깃. 새 코어부터 세우고 v1 코드를 이식하는 동안 두 트리가 공존한다.

---

## 3. Core: Type/Value 시스템 v2 (첫 서브시스템)

v2의 토대. 모든 상위 계층이 이 위에 선다.

### 3.1 Type
- `TypeTag { Exec, Bool, Int, Float, String, Object, Enum, Struct, Array, Set, Map }`.
- 타입은 `TypeRegistry`에 인턴되어 `TypeId`(정수 핸들)로 참조된다. 동일 타입은 dedup.
- `TypeDesc { tag, name(enum/struct/object), element(TypeId), key(TypeId) }`.
  - 컨테이너: `Array/Set`은 `element`, `Map`은 `key`+`element(value)`. 재귀 조합으로 `Array<Vector3f>`, `Map<String, Vector3f>` 표현.
- 사용자 타입 정의는 별도 저장:
  - `EnumDef { name, values[] }`
  - `StructDef { name, fields[]={name, TypeId} }`
- 헬퍼: `Builtin(tag)`, `ArrayOf(elem)`, `SetOf(elem)`, `MapOf(key,val)`, `UserType(name)`.

### 3.2 Value (통합, 재귀)
```
ValueData = variant<
  monostate,            // Exec / 값 없음(Object)
  bool, int64, double, string,
  StructVal{ fields: Value[] },
  ArrayVal { items:  Value[] },   // Array/Set
  MapVal   { entries: (Value,Value)[] }
>
```
- Enum 값 = `int64`(열거자 인덱스). Object = `monostate`(불투명 핸들, v2에서 확장).
- 하나의 `Value`가 실행 링크 전달과 프로퍼티 저장에 모두 쓰인다 → v1의 `PropertyValue.structFields` 특례 제거.
- 헬퍼: `MakeDefault(TypeId, registry)`, `ToString`, 동치 비교, `ToJson/FromJson(TypeId)`.

### 3.3 Result
- 예외 금지 정책에 맞춘 `Result<T>` / `Status`(코드+메시지) 경량 타입.

---

## 4. Model 계층 (v2)

- `Node`, `Pin`(TypeId 사용), `Link`, `CommentNode` — v1 계승하되 타입은 `TypeId`.
- `Graph`: 노드/링크 컨테이너. 서브그래프 지원을 위해 입력/출력 인터페이스 핀을 가진다.
- `Subgraph/FunctionDef`: 이름 + 인터페이스(입출력 핀) + 내부 `Graph`. 함수 노드는 이를 참조.
- `Project`: 다수 `Graph` + `TypeRegistry`(사용자 타입) + 설정 소유. 저장 단위.
- `Command/UndoStack`: v1 커맨드 패턴 계승. 모든 모델 변경은 커맨드로만.
- 직렬화: 스키마 버전 필드 + v1 -> v2 마이그레이션 어댑터.

---

## 5. Exec 계층 (v2)

- `Runtime`(VM): 중단 가능한 실행기. 프로그램 카운터/콜스택을 명시 상태로 보유해 스텝 실행.
- `DebugSession`: 브레이크포인트 집합, 실행 상태(Running/Paused/Done), watch 대상, 스텝/계속 제어.
- 값은 core `Value`로 흐른다 → struct/컨테이너 링크 전달(FR-EXE-3) 자연 지원.
- `BuiltinNodes` + 자동 생성 Make/Break(struct) + 변환 노드.

---

## 6. Render 계층

- v1 stateless 그리기 계승 (`Canvas`, `Grid`, `Node`, `Link`).
- 레이아웃 캐시는 `ui`가 소유하는 프레임 데이터로 이관(히트 테스트 공유).
- 타입 색: `TypeId` 기반, 사용자 타입은 해시 색(옵션으로 지정 색).

---

## 7. UI 계층 — 리테인드 UI

즉시모드의 rect 하드코딩을 대체한다.

- `Widget`: 보존 트리 노드. `Measure/Arrange`(레이아웃) + `Paint`(NanoVG) + `OnEvent`.
- 컨테이너/레이아웃: `Row`, `Column`, `Stack`, `Grid`, `Scroll`.
- 기본 위젯: `Button`, `Label`, `TextField`, `Dropdown`, `Panel`, `List`, `Splitter`.
- 도킹/플로팅 패널 프레임(프로퍼티/디버거/미니맵/라이브러리).
- 이벤트: `app`의 `InputRouter`가 정규화 이벤트를 위젯 트리로 히트 디스패치. 그래프 캔버스는 특수 위젯으로 편입하되 내부는 `interaction` FSM에 위임.

---

## 8. App / Platform

- `Application`: 창/NVG 컨텍스트, 프레임 루프, 활성 `Document` 관리.
- `Document`: 하나의 `Project` + 편집 상태(선택/뷰/undo).
- `InputRouter`: SDL 정규화 이벤트를 UI/캔버스로 라우팅.
- `platform/`: v1 SDL/NVG/입력 계승.
- `main.cpp`: `Application` 생성/실행만.

---

## 9. 마이그레이션 / 이행 순서

1. **core (Type/Value) + 테스트** ← 현재 착수.
2. model v2 (Graph/Project/Command) — core 위에 재구성, v1 직렬화 로더 이식.
3. exec VM + DebugSession.
4. ui 툴킷(리테인드) + 기본 위젯.
5. render 이식 + 캔버스 위젯.
6. interaction/app 조립, `main.cpp` 축소.
7. v1 파일 마이그레이션 검증.

각 단계는 독립 빌드 + 테스트 통과를 만족한 뒤 진행한다(v1 규약: 한 번에 한 단계).

---

## 10. 열린 결정 (추후 확정)
- Object 핸들의 런타임 표현(참조 카운팅/아레나).
- 리테인드 UI의 재-레이아웃 무효화 전략(더티 플래그 범위).
- 서브그래프 재귀 실행 시 스택 깊이/사이클 정책.
- Wasm 노드의 struct/다중 반환 ABI.
