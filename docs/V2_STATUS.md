# GAU v2 진행 상태 (핸드오프)

새 세션이 이어받기 위한 요약. 최신 브랜치: `main` (v1은 `v1` 브랜치 보존).

## 빌드 / 실행

```
cmake -S . -B build
cmake --build build --config Debug
ctest --test-dir build -C Debug        # 12개 스위트
build/Debug/gau.exe                     # v1 앱
build/Debug/gau2.exe                    # v2 앱
```

## v2 아키텍처 (그린필드, gau 네임스페이스, v1과 공존)

계층 라이브러리: `gau_core` -> `gau_model` -> `gau_exec` / `gau_render` /
`gau_ui` / `gau_interaction` / `gau_io`, app 은 `gau2`.

- **core** — TypeRegistry(인턴 타입, enum/struct/object, 컨테이너 Array/Set/Map,
  기본값, 표시명) + 통합 Value(스칼라/enum/struct/컨테이너). `core_tests`.
- **model** — Graph/Node/Link/NodeClassV2(레지스트리)/Function(FunctionDef +
  FunctionRegistry)/Project. TypeId 핀, Value 프로퍼티, CanConnect(타입+exec 사이클).
  `model_tests`.
- **exec** — 중단 가능 Runtime(Start/Step/Continue/Run, 브레이크포인트, exec 출력
  래치, 함수 호출 파라미터/결과 마샬링, 재귀 깊이 제한) + data pull +
  NodeEval/BuiltinRegistry + StructNodes(Make/Break 자동생성) + FunctionNodes(함수
  Entry/Return/Call 자동생성, 중첩 Runtime 실행) + FunctionOps(선택 노드 collapse ->
  함수, Call expand). `exec_vm_tests`, `struct_nodes_tests`, `function_nodes_tests`,
  `function_ops_tests`.
- **ui** — 리테인드 위젯(Widget/Column/Row/Panel/Label/Button), 추상 Painter.
  `ui_tests`.
- **render** — Canvas(팬/줌) + ComputeGraphLayout(추상 measure) + NanoVgPainter +
  DrawGraph/DrawSelection/DrawDragLink/DrawNodeOutline. `render_tests`.
- **interaction** — HitTest2 + InteractionFsm(드래그/링크/러버밴드). `interaction_tests`.
- **io** — V1Import(v1 custom_nodes.json/graph -> v2, ImportFunctions) +
  ExportProject(v2 -> 공유 JSON, functions 배열 포함, round-trip). `import_tests`,
  `project_io_tests`, `function_serialize_tests`.
- **app (gau2)** — 팔레트로 노드 생성(함수 Call 클래스 포함, 재빌드), 드래그 배치, 핀
  드래그 링크, shift+클릭 브레이크포인트, Debug/Step/Continue, Run(VM),
  Collapse(선택 -> 함수). 우드래그 팬 / 휠 줌.

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

전체 21개 스위트 통과. 위 로직은 라이브러리 + 테스트로 완성됐고, gau2 UI 배선(버튼/
패널/렌더)은 Collapse 만 연결됨 -- 나머지는 아래 참조.

## 남은 작업 (gau2 UI 배선, 로직은 준비됨)

1. **인라인 값 편집** — ui TextField 위젯 + 프로퍼티 패널(선택 노드 필드 편집). 유일하게
   로직도 미구현.
2. **UI 배선** — 아래 완성된 라이브러리를 gau2 에 연결(버튼/렌더):
   - 정렬(Align), 검색(NodeSearch), 미니맵(Minimap) 렌더, 코멘트 박스 렌더/드래그,
     타입 변환 자동 삽입(SuggestConversion, 링크 드롭 시), 변수 패널(Get/Set 생성),
     파일 저장/로드 다이얼로그(PlatformFileDialog 존재), Expand 버튼.
3. **함수 편집 UI** — 함수 본문 캔버스, 인터페이스 편집.

## 환경 제약으로 보류 (Windows 개발 환경서 검증 불가)

- **FR-WASM-1~3 커스텀/Wasm 노드** — v1 의 WasmRuntime(wasm3)/ExecEngine 은 존재하나
  v2 Runtime 에 미통합. 함수 런타임 빌드에 LLVM(clang wasm32) 필요. 배선은 가능하나
  실행 검증 불가.
- **FR-PLT-1~2 Apple/터치** (M12) — PlatformNVG 에 Metal(__APPLE__) 분기는 있으나
  .mm/ios 프로젝트 없음. macOS/Xcode/Metal 필요, Windows 서 빌드/실행 불가.

## 관례

- C++17, 예외 금지(반환값), ASCII 주석. 계층 의존 단방향.
- v2 파일명은 v1 과 충돌 회피(NodeClassV2/HitTest2/RenderCanvas 등), 모두 `gau` 네임스페이스.
- 각 슬라이스: 빌드 + 단위테스트 통과 후 커밋(`v2: ...`).
