cmake_minimum_required(VERSION 3.20)
project(eatscharted VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

if(MSVC)
    add_compile_options(/W4 /O2 /permissive- /wd4244 /wd4267 /wd4324 /openmp)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN)
elseif(EMSCRIPTEN)
    add_compile_options(-Wall -Wextra -O3 -fexceptions)
    add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN)
else()
    add_compile_options(-Wall -Wextra -O3)
    add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

# 1. Bundled GLFW 3.4 Windowing
if(NOT EMSCRIPTEN)
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
    add_subdirectory(third_party/glfw EXCLUDE_FROM_ALL)
endif()

# 2. Standalone Eatscript Scripting & Host ABI Library
add_library(eats_script STATIC
    src/eatscript/lexer.cpp
    src/eatscript/parser.cpp
    src/eatscript/vm.cpp
    src/eatscript/transpiler.cpp
    src/eatscript/evaluator.cpp
    src/eatscript/host_abi.cpp
    src/eatscript/stdlib.cpp
    src/eatscript/repl.cpp
    src/eatscript/dispatch_scanner.cpp
)
target_include_directories(eats_script PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# 3. Eatscharted 2.5D Living Cartography Core Library
add_library(eatscharted_core STATIC
    src/procgen/voronoi_atlas.cpp
    src/procgen/ink_canvas.cpp
    src/world/chart_world.cpp
    src/world/script_bindings.cpp
    src/audio/audio_foley.cpp
    src/ui/batch_renderer_2d.cpp
    src/ui/base64.cpp
    src/ui/monospace_font_8x16.cpp
    src/ui/theme.cpp
    src/ui/glyph/ebf_font.cpp
    src/ui/glyph/vector_font.cpp
    src/ui/glyph/glyph_packs.cpp
)
target_include_directories(eatscharted_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/miniaudio
)
target_link_libraries(eatscharted_core PUBLIC eats_script)

if(WIN32 AND NOT EMSCRIPTEN)
    target_link_libraries(eatscharted_core PUBLIC ole32 winmm user32 gdi32)
elseif(UNIX AND NOT APPLE AND NOT EMSCRIPTEN)
    target_link_libraries(eatscharted_core PUBLIC pthread dl m)
endif()

# 4. Eatscharted WebGPU / Dawn 2.5D Living Cartography Renderer Library
add_library(eatscharted_render STATIC
    src/render/chart_renderer.cpp
)
target_include_directories(eatscharted_render PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
if(NOT EMSCRIPTEN)
    target_include_directories(eatscharted_render PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/wgpu/include
    )
endif()

if(EMSCRIPTEN)
    target_compile_options(eatscharted_render PUBLIC --use-port=emdawnwebgpu)
    target_link_libraries(eatscharted_render PUBLIC eatscharted_core)
else()
    target_link_libraries(eatscharted_render PUBLIC eatscharted_core glfw)
    if(WIN32)
        set(WGPU_NATIVE_LIB "${CMAKE_CURRENT_SOURCE_DIR}/third_party/wgpu/lib/wgpu_native.dll.lib")
        target_link_libraries(eatscharted_render PUBLIC
            user32 gdi32 shell32 opengl32 "${WGPU_NATIVE_LIB}"
        )
    endif()
endif()

# 5. Main Eatscharted Executable
add_executable(eatscharted src/main.cpp)
target_link_libraries(eatscharted PRIVATE eatscharted_render)

if(WIN32 AND NOT EMSCRIPTEN)
    add_custom_command(TARGET eatscharted POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/third_party/wgpu/lib/wgpu_native.dll"
            "$<TARGET_FILE_DIR:eatscharted>/wgpu_native.dll"
    )
endif()

# 6. Automated Unit Test Suite (Disabled by default per AGENTS.md)
option(BUILD_TESTING "Build automated unit test suite" OFF)
if(BUILD_TESTING AND NOT EMSCRIPTEN)
    enable_testing()
    add_executable(eatscharted_tests tests/chart_core_tests.cpp)
    target_link_libraries(eatscharted_tests PRIVATE eatscharted_core)
    add_test(NAME eatscharted_tests COMMAND eatscharted_tests)
endif()
