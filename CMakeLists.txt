cmake_minimum_required(VERSION 3.21)
project(GAU LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# --- SDL3 (static) ---
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
add_subdirectory(external/SDL EXCLUDE_FROM_ALL)

# --- nanovg (static) ---
add_library(nanovg STATIC external/nanovg/src/nanovg.c)
target_include_directories(nanovg PUBLIC external/nanovg/src)
if(MSVC)
    target_compile_definitions(nanovg PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

# --- editor executable ---
add_executable(gau
    src/main.cpp
    src/platform/GLLoader.cpp
    src/platform/PlatformWindow.cpp
    src/platform/PlatformInput.cpp
    src/platform/PlatformNVG.cpp
    src/platform/PlatformFileDialog.cpp
    src/platform/PlatformConsole.cpp
    src/render/Canvas.cpp
    src/render/GridRenderer.cpp
    src/render/SelectionRenderer.cpp
    src/render/CommentRenderer.cpp
    src/render/TabBarRenderer.cpp
    src/render/PropertyPanelRenderer.cpp
    src/render/ActionMenuRenderer.cpp
    src/render/LogPanelRenderer.cpp
    src/render/NodeRenderer.cpp
    src/render/LinkRenderer.cpp
    src/render/ContextMenuRenderer.cpp
    src/render/ClassEditorDialogRenderer.cpp
    src/model/NodeGraph.cpp
    src/model/NodeClass.cpp
    src/model/BuiltinNodeClasses.cpp
    src/model/NodeClassLoader.cpp
    src/model/EditorSettings.cpp
    src/model/UndoStack.cpp
    src/model/GraphCommands.cpp
    src/model/GraphClipboard.cpp
    src/model/GraphSerializer.cpp
    src/model/PropertyText.cpp
    src/exec/ExecEngine.cpp
    src/exec/BuiltinNodes.cpp
    src/interaction/ActionMenu.cpp
    src/interaction/ContextMenu.cpp
    src/interaction/LogPanel.cpp
    src/interaction/PropertyPanel.cpp
    src/interaction/TabBar.cpp
    src/interaction/ClassEditorDialog.cpp
    src/interaction/HitTest.cpp
)
target_include_directories(gau PRIVATE src external)
target_link_libraries(gau PRIVATE SDL3::SDL3-static nanovg)
if(MSVC)
    target_compile_definitions(gau PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

# Keep the sample custom node file next to the executable so launching
# the exe directly (cwd = exe dir) also finds it.
add_custom_command(TARGET gau POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_SOURCE_DIR}/custom_nodes.json
            $<TARGET_FILE_DIR:gau>
)

# --- tests ---
enable_testing()
add_executable(canvas_tests
    tests/CanvasTests.cpp
    src/render/Canvas.cpp
)
target_include_directories(canvas_tests PRIVATE src)
add_test(NAME canvas_tests COMMAND canvas_tests)

add_executable(serializer_tests
    tests/SerializerTests.cpp
    src/model/NodeGraph.cpp
    src/model/NodeClass.cpp
    src/model/BuiltinNodeClasses.cpp
    src/model/GraphSerializer.cpp
)
target_include_directories(serializer_tests PRIVATE src external)
if(MSVC)
    target_compile_definitions(serializer_tests PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
add_test(NAME serializer_tests COMMAND serializer_tests)

add_executable(exec_tests
    tests/ExecTests.cpp
    src/model/NodeGraph.cpp
    src/model/NodeClass.cpp
    src/model/BuiltinNodeClasses.cpp
    src/exec/ExecEngine.cpp
    src/exec/BuiltinNodes.cpp
)
target_include_directories(exec_tests PRIVATE src)
if(MSVC)
    target_compile_definitions(exec_tests PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
add_test(NAME exec_tests COMMAND exec_tests)
