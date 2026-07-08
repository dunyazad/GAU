# GAU 재현 요구사항 — 다른 PC에서 동일 환경 구축

이 문서 하나로 새 PC에서 현재 저장소 상태(빌드/실행/테스트/기능)를 그대로
재현할 수 있도록 필요한 모든 요구사항을 정리한다.

- 기준 커밋: `1a6b407` (main 브랜치)
- 기준 일자: 2026-07-08

---

## 1. 필수 개발 환경

| 항목 | 요구사항 | 비고 |
|------|----------|------|
| OS | Windows 10/11 x64 | 현재 검증 플랫폼. macOS/iOS 는 미착수(FR-PLT-1~2) |
| 컴파일러 | Visual Studio 2022 (MSVC, C++17) | `run.bat` 이 `Visual Studio 17 2022 / x64` 제너레이터 고정 |
| CMake | 3.21 이상 | `cmake_minimum_required(VERSION 3.21)` |
| Git | 최신 | 서브모듈 필수 |
| LLVM clang (wasm32) | **선택** | 인앱 wasm 함수 빌드에만 필요. 미설치 시에도 기존 `.wasm` 실행은 됨 (wasm3 인터프리터) |

추가 SDK/런타임 불필요. OpenGL 3.3 은 Windows 그래픽 드라이버로 충족.

## 2. 저장소 가져오기

```
git clone --recursive <repo-url> GAU
cd GAU
# 이미 클론했다면:
git submodule update --init --recursive
```

### 2.1 외부 의존성 (이 4개 외 추가 금지 — CLAUDE.md 규약)

| 의존성 | 형태 | 고정 리비전 | 출처 |
|--------|------|-------------|------|
| SDL3 | git 서브모듈 `external/SDL` | `c9a6709` (release-3.2.16) | https://github.com/libsdl-org/SDL.git |
| NanoVG | git 서브모듈 `external/nanovg` | `ce3bf74` | https://github.com/memononen/nanovg.git |
| wasm3 | git 서브모듈 `external/wasm3` | `d77cd81` | https://github.com/wasm3/wasm3.git |
| nlohmann/json | 단일 헤더 벤더링 `external/nlohmann/json.hpp` | 저장소에 포함 | — |

서브모듈 리비전은 `.gitmodules` + 커밋에 고정되어 있으므로
`--recursive` 클론만으로 동일 버전이 재현된다.

## 3. 빌드 / 실행 / 테스트

```
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug          # 또는 Release
ctest --test-dir build -C Debug             # 전 스위트(약 29개) 통과해야 함
build/Debug/gau.exe                         # 통합 에디터 앱
```

편의 스크립트:

- `run.bat` — Release 구성으로 빌드 후 실행 (build 폴더 없으면 생성)
- `runwithoutbuild.bat` — 기존 `build/Release/gau.exe` 실행만

빌드 산출물: 정적 라이브러리 `gau_core, gau_model, gau_exec, gau_render,
gau_ui, gau_interaction, gau_io` + 앱 실행 파일 `gau.exe`.
CMake POST_BUILD 가 `wasm_src/` 와 (있으면) `tools/` 를 exe 옆에 복사한다.

성능 참고(NFR-1): 1000노드/1798링크 프로젝트에서 Release 는 60fps(16.7ms
vsync 고정), Debug 는 약 34ms/frame. 성능 확인은 Release 로 할 것.

## 4. 저장소 구성 (무엇이 어디에 있는가)

```
src/
  core/         # TypeRegistry(인턴 타입), Value(스칼라/enum/struct/컨테이너)
  model/        # Graph/Node/Link/NodeClassV2/Function/Variable/Comment/Project, UndoHistory
  exec/         # 중단 가능 Runtime(VM), Builtins, Struct/Function/Conversion/Variable 노드,
                # WasmHost(wasm3)/WasmNodes/WasmAuthoring/WasmBuild 파이프라인
  render/       # Canvas(팬/줌), 그래프 레이아웃/그리기 (NanoVG, stateless)
  ui/           # 리테인드 위젯 툴킷 (백엔드 무관 Painter 추상)
  interaction/  # HitTest2, InteractionFsm, Align, NodeSearch, Minimap, 다이얼로그
  io/           # V1Import, ExportProject, ProjectFile (schemaVersion=2)
  app/          # gau 앱 조립 (Main.cpp, WasmBuild)
  platform/     # SDL 창/입력/NVG 컨텍스트 (플랫폼 분기 유일 허용 계층)
tests/          # 계층별 단위 테스트 (ctest 등록)
external/       # SDL, nanovg, nlohmann, wasm3
docs/           # 본 문서 + 아래 5절 문서들
```

### 4.1 루트 데이터 파일 (git 포함, 재현에 필요)

| 파일/디렉터리 | 역할 |
|---------------|------|
| `main.json`, `Test.json` | 예제/테스트 그래프 (앱에서 Load 로 열기) |
| `custom_nodes.json` | 사용자 정의 노드 클래스 정의 |
| `editor_settings.json` | 에디터 설정. `tools.clangPath` 로 clang 경로 지정 가능 |
| `gau_session.json` | 세션 상태(창/뷰/패널 토글). 실행 시 자동 갱신 |
| `wasm/` | 빌드된 `.wasm` 모듈 (앱이 시작 시 스캔·로드) |
| `wasm_src/` | wasm 함수 C++ 소스 + 생성된 `gau_api.h` |
| `exec_test_wasm/adder.wasm` | 단위 테스트용 수제 wasm (clang 불필요) |
| `.gau_autosave.json` | 자동 저장 파일(30초 주기, 크래시 복구용). git 미포함, 런타임 생성 |

## 5. 기능 명세 문서 (구현 기준의 단일 소스)

기능을 "무엇을 만들었는가" 수준까지 재현하려면 아래 문서를 이 순서로 읽는다.

1. `CLAUDE.md` — 코딩 규약, 아키텍처 원칙, 블루프린트 디자인 스펙(색/치수),
   인터랙션 FSM, 실행 엔진 스펙, v1 마일스톤 M0~M6.
2. `docs/SRS_v2.md` — v2 기능 요구사항(FR-EXE/TYP/REU/UX/PRJ/WASM/PLT) + NFR.
3. `docs/ARCHITECTURE_v2.md` — v2 계층 설계(core→model→exec/render/ui/interaction→app).
4. `docs/V2_STATUS.md` — **현재 구현 완료 상태의 단일 소스.** 어떤 FR 이
   완료됐고 어떤 테스트가 커버하는지, gau UI 배선 현황, 남은 항목.
5. `docs/WASM_BUNDLING.md` — clang 툴체인 탐색 순서/번들 레이아웃(FR-WASM-4).

주의: 루트의 `checklist.md` / `context-notes.md` 는 v1 잔재 — 무시한다.

### 5.1 현재 상태 요약

- v1 파리티(데스크톱 UX) 전 항목 완료: 우클릭 메뉴, 툴바/패널, 러버밴드,
  삭제/복사/붙여넣기, alt+클릭·Ctrl+드래그 링크 절단, 자동 저장+크래시 복구,
  종료 시 미저장 확인, 세션 유지, 미니맵, 검색, 자동 정렬, 인앱 wasm 저작.
- v2 기능 로직 + 단위 테스트 완료: 함수 노드(collapse/expand), 지역 변수,
  타입 변환, struct Make/Break + 편집 재동기화, 프로젝트 파일(schemaVersion=2),
  디버깅(브레이크포인트/스텝), wasm 런타임 통합(FlatWasmContext struct 평탄화).
- 미착수: FR-PLT-1~2 (macOS/iOS + 터치) — macOS/Xcode 환경 필요, Windows 불가.

## 6. Wasm 툴체인 (선택 — 인앱 함수 빌드 시에만)

앱은 아래 순서로 clang 을 탐색한다 (`FindClangForWasm`):

1. `editor_settings.json` 의 `tools.clangPath`
2. `<exe 디렉터리>/tools/llvm/bin/clang.exe` (번들 위치)
3. `<exe 디렉터리>/tools/clang.exe`
4. `tools/llvm/bin/clang.exe`, `tools/clang.exe` (작업 디렉터리 상대)
5. `C:/Program Files/LLVM/bin/clang.exe`
6. PATH 의 `clang`

새 PC 에서 가장 간단한 방법: 공식 LLVM(Windows x64) 설치 →
`C:/Program Files/LLVM` 이 자동 탐색된다. 무설치 재현은
`tools/llvm/bin/` 에 `clang.exe` + `wasm-ld.exe` 를 넣는다
(`tools/` 는 `.gitignore` 대상 — 수동 복사 필요, `docs/WASM_BUNDLING.md` 참조).

빌드 커맨드는 앱이 자동 생성한다:

```
clang --target=wasm32 -nostdlib -O2 -std=c++17 -fno-exceptions -fno-rtti \
  -Wl,--no-entry -Wl,--export-all -Wl,--allow-undefined \
  -o wasm/<name>.wasm wasm_src/<name>.cpp
```

## 7. 재현 검증 체크리스트

새 PC 에서 아래가 모두 통과하면 동일 환경 재현 완료다.

1. `git clone --recursive` 후 `git submodule status` 가 2.1절 리비전과 일치.
2. `cmake -S . -B build` + `cmake --build build --config Debug` 오류 없음.
3. `ctest --test-dir build -C Debug` 전 스위트 통과.
4. `gau.exe` 실행 → 그리드/툴바/미니맵 표시, 우클릭 팬, 휠 커서 고정 줌.
5. `Test.json` 또는 `main.json` Load → 그래프 복원, Run 으로 실행 동작.
6. wasm 노드 실행: 시작 시 `wasm/` 모듈 로드, 그래프에서 wasm 노드 평가 동작
   (clang 불필요 — 실행은 wasm3 인터프리터).
7. (clang 설치 시) Wasm 패널 Edit Function → Build & Save → `.wasm` 생성 및
   팔레트 반영.
8. 강제 종료 후 재시작 → `.gau_autosave.json` 복구 제안(Restore/Discard) 표시.
