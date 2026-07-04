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
- **model** — Graph/Node/Link/NodeClassV2(레지스트리)/Project. TypeId 핀, Value
  프로퍼티, CanConnect(타입+exec 사이클). `model_tests`.
- **exec** — 중단 가능 Runtime(Start/Step/Continue/Run, 브레이크포인트) + data
  pull + NodeEval/BuiltinRegistry + StructNodes(Make/Break 자동생성).
  `exec_vm_tests`, `struct_nodes_tests`.
- **ui** — 리테인드 위젯(Widget/Column/Row/Panel/Label/Button), 추상 Painter.
  `ui_tests`.
- **render** — Canvas(팬/줌) + ComputeGraphLayout(추상 measure) + NanoVgPainter +
  DrawGraph/DrawSelection/DrawDragLink/DrawNodeOutline. `render_tests`.
- **interaction** — HitTest2 + InteractionFsm(드래그/링크/러버밴드). `interaction_tests`.
- **io** — V1Import(v1 custom_nodes.json/graph -> v2) + ExportProject(v2 -> 공유
  JSON, round-trip). `import_tests`, `project_io_tests`.
- **app (gau2)** — 팔레트로 노드 생성, 드래그 배치, 핀 드래그 링크, shift+클릭
  브레이크포인트, Debug/Step/Continue, Run(VM). 우드래그 팬 / 휠 줌.

## 남은 작업 (우선순위 순)

1. **서브그래프 / 함수 노드 실행** (FR-REU-1) — 함수 정의(이름+인터페이스+내부 그래프),
   Call 노드, 재귀 Runtime 호출로 입출력 마샬링. 가장 큰 슬라이스.
2. **인라인 값 편집** — ui 에 TextField 위젯 추가, 프로퍼티/핀 기본값 편집. gau2 에
   프로퍼티 패널(선택 노드 필드 편집).
3. **타입 변환 노드 / 자동 변환 제안** (FR-TYP-3).
4. **UX** — 미니맵, 노드 검색/포커스, 정렬(6방향/분배)을 v2 로 이식, 코멘트 박스.
5. **직렬화 파일 I/O** — ExportProject 결과를 파일 저장 + 로드 UI, 스키마 버전 필드.
6. **Apple/터치** (M12).

## 관례

- C++17, 예외 금지(반환값), ASCII 주석. 계층 의존 단방향.
- v2 파일명은 v1 과 충돌 회피(NodeClassV2/HitTest2/RenderCanvas 등), 모두 `gau` 네임스페이스.
- 각 슬라이스: 빌드 + 단위테스트 통과 후 커밋(`v2: ...`).
