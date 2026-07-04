# GAU v2 진행 상태 (핸드오프)

새 세션이 이어받기 위한 요약. 최신 브랜치: `main` (v1은 `v1` 브랜치 보존).

## 빌드 / 실행

```
cmake -S . -B build
cmake --build build --config Debug
ctest --test-dir build -C Debug        # 22개 스위트, 전부 통과
build/Debug/gau.exe                     # v1 앱
build/Debug/gau2.exe                    # v2 앱
```

루트의 `checklist.md` / `context-notes.md` 는 v1 사용자 타입 작업 잔재라 무시. v2 핸드오프는
이 문서가 단일 소스.

## v2 아키텍처 (그린필드, gau 네임스페이스, v1과 공존)

계층 라이브러리: `gau_core` -> `gau_model` -> `gau_exec` / `gau_render` /
`gau_ui` / `gau_interaction` / `gau_io`, app 은 `gau2`.

- **core** — TypeRegistry(인턴 타입, enum/struct/object, 컨테이너 Array/Set/Map,
  기본값, 표시명) + 통합 Value(스칼라/enum/struct/컨테이너). `core_tests`.
- **model** — Graph/Node/Link/NodeClassV2(레지스트리)/Function(FunctionDef +
  FunctionRegistry)/Variable/Comment/Project + UndoHistory(그래프 스냅샷 언두/리두).
  TypeId 핀, Value 프로퍼티, CanConnect(타입+exec 사이클). `model_tests`,
  `undo_history_tests`.
- **exec** — 중단 가능 Runtime(Start/Step/Continue/Run, 브레이크포인트, exec 출력
  래치, 함수 호출 파라미터/결과 마샬링, 재귀 깊이 제한, 변수 저장소, RunError 진단) +
  data pull + NodeEval/BuiltinRegistry + StructNodes(Make/Break) + FunctionNodes(함수
  Entry/Return/Call 자동생성, 중첩 Runtime) + FunctionOps(collapse/expand) +
  ConversionNodes(스칼라 변환 + SuggestConversion) + VariableNodes(Get/Set).
  `exec_vm_tests`, `struct_nodes_tests`, `function_nodes_tests`, `function_ops_tests`,
  `conversion_nodes_tests`, `variable_nodes_tests`.
- **ui** — 리테인드 위젯(Widget/Column/Row/Panel/Label/Button/TextField), 추상 Painter.
  `ui_tests`.
- **render** — Canvas(팬/줌) + ComputeGraphLayout(추상 measure) + NanoVgPainter +
  DrawGraph/DrawSelection/DrawDragLink/DrawNodeOutline. `render_tests`.
- **interaction** — HitTest2 + InteractionFsm(드래그/링크/러버밴드) + Align(정렬/분배) +
  NodeSearch(검색/바운드) + Minimap(fit 변환 + NodesInRect 그룹핑).
  `interaction_tests`, `align_tests`, `node_search_tests`, `minimap_tests`.
- **io** — V1Import(v1 -> v2, ImportFunctions/Variables/Comments) + ExportProject(v2 ->
  공유 JSON: functions/variables/comments/schemaVersion) + ProjectFile(파일 save/load +
  LoadProjectText). `import_tests`, `project_io_tests`, `function_serialize_tests`,
  `project_file_tests`.
- **app (gau2)** — Project 컨테이너 편집. 팔레트 노드 생성(함수 Call + 변환 노드, 재빌드),
  드래그 배치, 핀 드래그 링크, 프로퍼티 패널(인라인 값 편집), Collapse/Expand, 정렬/분배,
  Comment 추가, 검색(뷰 센터 이동), 미니맵/코멘트 렌더, Save/Load(네이티브 다이얼로그),
  shift+클릭 브레이크포인트, Debug/Step/Continue/Run. 우드래그 팬 / 휠 줌.

## 완료 (기능 로직 + 단위 테스트)

- **FR-REU-1 서브그래프/함수 노드** — FunctionDef/FunctionRegistry, FunctionNodes
  (Entry/Return/Call 자동생성 + 중첩 Runtime 마샬링, 재귀 깊이 제한, exec 출력 래치),
  FunctionOps(collapse/expand), 직렬화, gau2 Collapse 버튼 + 팔레트.
  `function_nodes_tests`, `function_ops_tests`, `function_serialize_tests`.
- **FR-TYP-3 타입 변환 노드** — ConversionNodes(스칼라 변환 빌트인 + SuggestConversion).
  `conversion_nodes_tests`.
- **FR-REU-2 지역 변수** — VariableDef(Project) + VariableNodes(Get/Set) + Runtime
  변수 저장소(스텝 간 유지, Start 시 초기화). `variable_nodes_tests`.
- **FR-EXE-4 실행 오류 진단** — RunError(kind+node+message): NodeNotFound,
  StepLimitExceeded. `exec_vm_tests`.
- **FR-PRJ-1/3 프로젝트 파일 + 스키마 버전** — ProjectFile(SaveProjectFile/
  LoadProjectFile/LoadProjectText), schemaVersion=2, 함수/변수/코멘트 직렬화, 암시적
  마이그레이션. `project_file_tests`.
- **FR-UX-3 정렬** — Align(6방향 + 분배, 순수 좌표). `align_tests`.
- **FR-UX-2 검색/포커스** — NodeSearch(부분일치 + 포커스 바운드). `node_search_tests`.
- **FR-UX-1 미니맵 + FR-UX-4 코멘트** — Minimap(fit 변환 + NodesInRect 그룹핑),
  Comment 모델 + 직렬화. `minimap_tests`.

전체 21개 스위트 통과. 위 기능 로직은 라이브러리 + 테스트로 완성. gau2 UI 배선 현황은
아래 참조.

## gau2 UI 배선

- 완료:
  - 인라인 값 편집(ui TextField + 프로퍼티 패널, `ui_tests`).
  - Collapse/Expand 버튼, 정렬(Align L/T)/분배(Distribute H) 버튼.
  - 함수 팔레트 재빌드(변환 노드 포함), Debug/Step/Continue/Run.
  - 프로젝트 Save/Load(네이티브 다이얼로그 + ProjectFile). gau2 는 이제 Project 컨테이너
    사용, 로드 시 레지스트리 in-place Clear -> 재임포트 -> 동작 재바인딩 -> 팔레트 재빌드.
  - 미니맵 렌더(하단 중앙, 노드 + 뷰포트), 코멘트 박스 렌더(Comment 버튼으로 추가),
    검색창(상단 중앙, 매칭 노드로 뷰 센터 이동).
  - 언두/리두: 그래프 편집 전 UndoHistory.Record. Ctrl+Z/Shift+Z/Ctrl+Y + Undo/Redo 버튼.
    노드 추가/드래그/링크/collapse/expand/정렬/프로퍼티 커버(코멘트는 그래프 밖이라 제외).
  - 타입 변환 자동 삽입: 비호환 핀 드롭 시 InsertConversion 으로 변환 노드 삽입.
  - 변수 패널(상단 좌측, int 변수 추가 -> Get/Set 팔레트 등록).
  - 미니맵 클릭 뷰 네비게이션, 코멘트 드래그(포함 노드 함께 이동, 노드 이동은 언두 커버).
- 남은 배선:
  - 함수 편집 UI(본문 캔버스 전환, 인터페이스 편집). 큰 슬라이스 -- 아래 참고.
  - 변수 삭제/타입 지정, 코멘트 편집/삭제(현재 추가/이동만).

## 환경 제약으로 보류 (Windows 개발 환경서 검증 불가)

- **FR-WASM-1~3 커스텀/Wasm 노드** — v1 의 WasmRuntime(wasm3)/ExecEngine 은 존재하나
  v2 Runtime 에 미통합. 함수 런타임 빌드에 LLVM(clang wasm32) 필요. 배선은 가능하나
  실행 검증 불가.
- **FR-PLT-1~2 Apple/터치** (M12) — PlatformNVG 에 Metal(__APPLE__) 분기는 있으나
  .mm/ios 프로젝트 없음. macOS/Xcode/Metal 필요, Windows 서 빌드/실행 불가.

## 다음 세션 착수 순서 (권장)

1. **함수 편집 UI** — 함수 본문(def->body)을 메인 캔버스에서 편집. 착수 노트:
   gau2 는 현재 `Graph& graph = *project.graph` 고정 참조로 편집. 함수 본문 편집하려면
   활성 그래프를 `Graph*` 포인터로 바꿔 메인/함수 본문 전환 필요(전 사용처 `graph` ->
   `*activeGraph` 치환, 큰 churn). Call 노드 선택 -> "Edit Fn" 으로 본문 진입, "Main"
   으로 복귀. 언두 히스토리도 그래프별로 분리 필요.
2. **정리 항목** — 변수 삭제/타입 지정, 코멘트 편집/삭제.
3. (환경 갖춰지면) **FR-WASM** (LLVM 필요), **FR-PLT Apple/터치** (macOS/Xcode 필요).

언두 관련 알려진 한계: UndoHistory 는 그래프 스냅샷만 -> collapse/expand 언두 시 그래프는
복원되나 생성된 함수 클래스/def 는 레지스트리에 잔류(미사용 orphan). 코멘트/변수 정의는
그래프 밖이라 언두 미커버. 필요 시 Project 레벨 히스토리로 확장.

## 관례

- C++17, 예외 금지(반환값), ASCII 주석. 계층 의존 단방향.
- v2 파일명은 v1 과 충돌 회피(NodeClassV2/HitTest2/RenderCanvas 등), 모두 `gau` 네임스페이스.
- 각 슬라이스: 빌드 + 단위테스트 통과 후 커밋(`v2: ...`).
- 함수/변수 로드 후에는 반드시 동작 재바인딩(RegisterFunctionNodes/RegisterVariableNodes/
  RegisterStructNodes/RegisterConversionNodes) 필요 -- io 계층은 데이터만 로드(exec 의존 금지).
