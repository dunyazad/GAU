# Wasm 툴체인 번들 (FR-WASM-4)

사용자가 LLVM 을 따로 설치하지 않고도 GAU 안에서 새 wasm 노드 함수를 추가·빌드·적용할 수
있도록, wasm32 를 컴파일하는 clang 툴체인을 배포본에 포함한다.

## 앱의 clang 탐색 순서

`FindClangForWasm` (`src/main.cpp`) 이 아래 순서로 찾는다.

1. `EditorSettings.clangPath` (설정값, `editor_settings.json`)
2. `<exe 디렉터리>/tools/llvm/bin/clang.exe`  ← 번들 위치(우선)
3. `<exe 디렉터리>/tools/clang.exe`
4. `tools/llvm/bin/clang.exe` (작업 디렉터리 상대)
5. `tools/clang.exe`
6. `C:/Program Files/LLVM/bin/clang.exe` (시스템 설치)
7. `clang` (PATH)

실행 파일 위치는 `argv[0]` 로 구해 `g_appDir` 에 저장하므로, 어느 디렉터리에서 실행해도
번들 clang 을 찾는다.

## 번들 레이아웃 (배포본)

```
gau.exe
custom_nodes.json
wasm_src/            # 함수 소스 + 생성된 gau_api.h
wasm/                # 빌드된 .wasm 모듈 (앱이 씀)
tools/
  llvm/
    bin/
      clang.exe
      wasm-ld.exe    # clang 이 링크 시 호출
```

- CMake POST_BUILD 가 `wasm_src/` 와 (있으면) `tools/` 를 exe 옆으로 복사한다.
- `tools/` 는 용량이 커서 git 에 넣지 않는다(`.gitignore` 의 `/tools/`). 패키징 단계에서
  wasm32 가능한 clang 을 `tools/llvm/bin/` 에 넣는다.

## 최소 툴체인 구성

전체 LLVM 대신 wasm 빌드에 필요한 것만 추린다.

- `clang.exe` (또는 `clang++.exe`)
- `wasm-ld.exe`
- clang 내장 헤더(`lib/clang/<ver>/include`) — freestanding 빌드라 표준 라이브러리는
  불필요하지만 컴파일러 내장 헤더 경로는 필요할 수 있다.

빌드 커맨드(참고, `BuildWasmFunction`):

```
clang --target=wasm32 -nostdlib -O2 -std=c++17 -fno-exceptions -fno-rtti \
  -Wl,--no-entry -Wl,--export-all -Wl,--allow-undefined \
  -o wasm/<name>.wasm wasm_src/<name>.cpp
```

## 라이선스

LLVM/clang 은 Apache-2.0 with LLVM exceptions. 번들 시 라이선스 고지를 배포본에 포함한다.

## 검증 (AC)

LLVM 미설치 환경에서:

1. 번들 레이아웃으로 `gau.exe` 실행.
2. New Wasm Function -> 소스 작성 -> Build.
3. `wasm/<name>.wasm` 생성 + `@node` 로 노드 클래스 등록 + 그래프 실행에서 동작.

외부 설치·설정 없이 위가 되면 FR-WASM-4 충족.
