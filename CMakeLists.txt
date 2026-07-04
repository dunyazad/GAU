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

# --- wasm3 (static) ---
file(GLOB WASM3_SOURCES external/wasm3/source/*.c)
add_library(wasm3 STATIC ${WASM3_SOURCES})
target_include_directories(wasm3 PUBLIC external/wasm3/source)
if(MSVC)
    target_compile_definitions(wasm3 PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

# --- gau_core (v2 type/value system) ---
add_library(gau_core STATIC
    src/core/Value.cpp
    src/core/TypeRegistry.cpp
)
target_include_directories(gau_core PUBLIC src)
if(MSVC)
    target_compile_definitions(gau_core PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

# --- gau_model (v2 graph/node model) ---
add_library(gau_model STATIC
    src/model/Graph.cpp
    src/model/NodeClassV2.cpp
    src/model/Function.cpp
)
target_include_directories(gau_model PUBLIC src)
target_link_libraries(gau_model PUBLIC gau_core)
if(MSVC)
    target_compile_definitions(gau_model PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

# --- gau_exec (v2 runtime VM) ---
add_library(gau_exec STATIC
    src/exec/Runtime.cpp
    src/exec/Builtins.cpp
    src/exec/StructNodes.cpp
    src/exec/FunctionNodes.cpp
    src/exec/FunctionOps.cpp
    src/exec/ConversionNodes.cpp
    src/exec/VariableNodes.cpp
)
target_include_directories(gau_exec PUBLIC src)
target_link_libraries(gau_exec PUBLIC gau_model gau_core)
if(MSVC)
    target_compile_definitions(gau_exec PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

# --- gau_ui (v2 retained UI toolkit, backend-agnostic) ---
add_library(gau_ui STATIC
    src/ui/Widget.cpp
)
target_include_directories(gau_ui PUBLIC src)
if(MSVC)
    target_compile_definitions(gau_ui PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

# --- gau_render (v2 canvas/layout + NanoVG painter) ---
add_library(gau_render STATIC
    src/render/RenderCanvas.cpp
    src/render/GraphLayout.cpp
    src/render/NanoVgPainter.cpp
    src/render/GraphRenderer.cpp
)
target_include_directories(gau_render PUBLIC src)
target_link_libraries(gau_render PUBLIC gau_model gau_core gau_ui nanovg)
if(MSVC)
    target_compile_definitions(gau_render PRIVATE _CRT_SECURE_NO_WARNINGS)
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
    src/platform/PlatformProcess.cpp
    src/platform/PlatformClipboard.cpp
    src/render/Canvas.cpp
    src/render/GridRenderer.cpp
    src/render/SelectionRenderer.cpp
    src/render/CommentRenderer.cpp
    src/render/TabBarRenderer.cpp
    src/render/PropertyPanelRenderer.cpp
    src/render/ActionMenuRenderer.cpp
    src/render/LogPanelRenderer.cpp
    src/render/FunctionEditorRenderer.cpp
    src/render/NodeRenderer.cpp
    src/render/LinkRenderer.cpp
    src/render/ContextMenuRenderer.cpp
    src/render/ClassEditorDialogRenderer.cpp
    src/model/NodeGraph.cpp
    src/model/NodeClass.cpp
    src/model/UserType.cpp
    src/model/BuiltinNodeClasses.cpp
    src/model/NodeClassLoader.cpp
    src/model/EditorSettings.cpp
    src/model/Guid.cpp
    src/model/UndoStack.cpp
    src/model/GraphCommands.cpp
    src/model/GraphClipboard.cpp
    src/model/GraphSerializer.cpp
    src/model/PropertyText.cpp
    src/exec/ExecEngine.cpp
    src/exec/BuiltinNodes.cpp
    src/exec/WasmRuntime.cpp
    src/exec/WasmApiHeader.cpp
    src/interaction/ActionMenu.cpp
    src/interaction/ContextMenu.cpp
    src/interaction/FunctionEditorDialog.cpp
    src/interaction/LogPanel.cpp
    src/interaction/PropertyPanel.cpp
    src/interaction/TabBar.cpp
    src/interaction/ClassEditorDialog.cpp
    src/interaction/HitTest.cpp
    src/interaction/NodeAlign.cpp
)
target_include_directories(gau PRIVATE src external)
target_link_libraries(gau PRIVATE SDL3::SDL3-static nanovg wasm3)
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

# --- gau_interaction (v2 hit test + interaction FSM) ---
add_library(gau_interaction STATIC
    src/interaction/HitTest2.cpp
    src/interaction/InteractionFsm.cpp
)
target_include_directories(gau_interaction PUBLIC src)
target_link_libraries(gau_interaction PUBLIC gau_render gau_model gau_core)
if(MSVC)
    target_compile_definitions(gau_interaction PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

# --- gau_io (v2 serialization / v1 import) ---
add_library(gau_io STATIC
    src/io/V1Import.cpp
    src/io/ProjectExport.cpp
)
target_include_directories(gau_io PUBLIC src external)
target_link_libraries(gau_io PUBLIC gau_model gau_core)
if(MSVC)
    target_compile_definitions(gau_io PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

# --- gau2 (v2 app: assembles the new stack on the v1 platform) ---
add_executable(gau2
    src/app/Main.cpp
    src/platform/GLLoader.cpp
    src/platform/PlatformWindow.cpp
    src/platform/PlatformNVG.cpp
    src/platform/PlatformInput.cpp
)
target_include_directories(gau2 PRIVATE src external)
target_link_libraries(gau2 PRIVATE SDL3::SDL3-static nanovg gau_render gau_exec gau_interaction)
if(MSVC)
    target_compile_definitions(gau2 PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

# --- tests ---
enable_testing()

add_executable(core_tests tests/CoreTests.cpp)
target_include_directories(core_tests PRIVATE src)
target_link_libraries(core_tests PRIVATE gau_core)
add_test(NAME core_tests COMMAND core_tests)

add_executable(model_tests tests/ModelTests.cpp)
target_include_directories(model_tests PRIVATE src)
target_link_libraries(model_tests PRIVATE gau_model)
add_test(NAME model_tests COMMAND model_tests)

add_executable(exec_vm_tests tests/ExecVmTests.cpp)
target_include_directories(exec_vm_tests PRIVATE src)
target_link_libraries(exec_vm_tests PRIVATE gau_exec)
add_test(NAME exec_vm_tests COMMAND exec_vm_tests)

add_executable(struct_nodes_tests tests/StructNodesTests.cpp)
target_include_directories(struct_nodes_tests PRIVATE src)
target_link_libraries(struct_nodes_tests PRIVATE gau_exec)
add_test(NAME struct_nodes_tests COMMAND struct_nodes_tests)

add_executable(function_nodes_tests tests/FunctionNodesTests.cpp)
target_include_directories(function_nodes_tests PRIVATE src)
target_link_libraries(function_nodes_tests PRIVATE gau_exec)
add_test(NAME function_nodes_tests COMMAND function_nodes_tests)

add_executable(function_ops_tests tests/FunctionOpsTests.cpp)
target_include_directories(function_ops_tests PRIVATE src)
target_link_libraries(function_ops_tests PRIVATE gau_exec)
add_test(NAME function_ops_tests COMMAND function_ops_tests)

add_executable(conversion_nodes_tests tests/ConversionNodesTests.cpp)
target_include_directories(conversion_nodes_tests PRIVATE src)
target_link_libraries(conversion_nodes_tests PRIVATE gau_exec)
add_test(NAME conversion_nodes_tests COMMAND conversion_nodes_tests)

add_executable(variable_nodes_tests tests/VariableNodesTests.cpp)
target_include_directories(variable_nodes_tests PRIVATE src)
target_link_libraries(variable_nodes_tests PRIVATE gau_exec)
add_test(NAME variable_nodes_tests COMMAND variable_nodes_tests)

add_executable(ui_tests tests/UiTests.cpp)
target_include_directories(ui_tests PRIVATE src)
target_link_libraries(ui_tests PRIVATE gau_ui)
add_test(NAME ui_tests COMMAND ui_tests)

add_executable(render_tests tests/RenderTests.cpp)
target_include_directories(render_tests PRIVATE src)
target_link_libraries(render_tests PRIVATE gau_render)
add_test(NAME render_tests COMMAND render_tests)

add_executable(import_tests tests/ImportTests.cpp)
target_include_directories(import_tests PRIVATE src external)
target_link_libraries(import_tests PRIVATE gau_io)
add_test(NAME import_tests COMMAND import_tests)

add_executable(project_io_tests tests/ProjectIoTests.cpp)
target_include_directories(project_io_tests PRIVATE src external)
target_link_libraries(project_io_tests PRIVATE gau_io)
add_test(NAME project_io_tests COMMAND project_io_tests)

add_executable(function_serialize_tests tests/FunctionSerializeTests.cpp)
target_include_directories(function_serialize_tests PRIVATE src external)
target_link_libraries(function_serialize_tests PRIVATE gau_exec gau_io)
add_test(NAME function_serialize_tests COMMAND function_serialize_tests)

add_executable(interaction_tests tests/InteractionTests.cpp)
target_include_directories(interaction_tests PRIVATE src)
target_link_libraries(interaction_tests PRIVATE gau_interaction)
add_test(NAME interaction_tests COMMAND interaction_tests)

add_executable(canvas_tests
    tests/CanvasTests.cpp
    src/render/Canvas.cpp
)
target_include_directories(canvas_tests PRIVATE src)
add_test(NAME canvas_tests COMMAND canvas_tests)

add_executable(serializer_tests
    tests/SerializerTests.cpp
    src/model/NodeGraph.cpp
    src/model/Guid.cpp
    src/model/NodeClass.cpp
    src/model/UserType.cpp
    src/model/BuiltinNodeClasses.cpp
    src/model/GraphSerializer.cpp
    src/model/PropertyText.cpp
)
target_include_directories(serializer_tests PRIVATE src external)
if(MSVC)
    target_compile_definitions(serializer_tests PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
add_test(NAME serializer_tests COMMAND serializer_tests)

add_executable(exec_tests
    tests/ExecTests.cpp
    src/model/NodeGraph.cpp
    src/model/Guid.cpp
    src/model/NodeClass.cpp
    src/model/UserType.cpp
    src/model/BuiltinNodeClasses.cpp
    src/model/PropertyText.cpp
    src/exec/ExecEngine.cpp
    src/exec/BuiltinNodes.cpp
    src/exec/WasmRuntime.cpp
)
target_include_directories(exec_tests PRIVATE src)
target_link_libraries(exec_tests PRIVATE wasm3)
if(MSVC)
    target_compile_definitions(exec_tests PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
add_test(NAME exec_tests COMMAND exec_tests)
