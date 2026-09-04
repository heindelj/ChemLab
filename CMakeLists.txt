cmake_minimum_required(VERSION 3.18)
project(ChemLab LANGUAGES C CXX)

include(FetchContent)

# Default to RelWithDebInfo: optimised, but with symbols for a usable
# backtrace. CMake's RelWithDebInfo also defines NDEBUG, which silences every
# IM_ASSERT in ImGui / the node editor / ImPlot -- exactly the checks that
# turn a mystery EXC_BAD_ACCESS into a readable message -- so strip it.
# Use -DCMAKE_BUILD_TYPE=Release for a no-asserts build.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()
foreach(lang C CXX)
    string(REPLACE "-DNDEBUG" "" CMAKE_${lang}_FLAGS_RELWITHDEBINFO "${CMAKE_${lang}_FLAGS_RELWITHDEBINFO}")
endforeach()
# -DCHEMLAB_ASAN=ON: AddressSanitizer for the whole build (used by the
# headless smoke tests, see CLAUDE.md).
option(CHEMLAB_ASAN "Build with AddressSanitizer" OFF)
if(CHEMLAB_ASAN)
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT EMSCRIPTEN)
    set(THREADS_PREFER_PTHREAD_FLAG ON)
    find_package(Threads REQUIRED)
endif()

# ---------------------------------------------------------------------------
# Web (Emscripten) build. Configure with:
#     emcmake cmake -B build-web -DCMAKE_BUILD_TYPE=Release && cmake --build build-web
# The impostor renderer needs gl_FragDepth, flat varyings and instanced arrays,
# which are core in GLSL ES 3.00 but not in ES 1.00, so the web build targets
# WebGL2 only. raylib adds -sMIN/MAX_WEBGL_VERSION=2 itself when it is built
# with OPENGL_VERSION="ES 3.0".
# ---------------------------------------------------------------------------
if(EMSCRIPTEN)
    set(PLATFORM Web CACHE STRING "" FORCE)
    set(OPENGL_VERSION "ES 3.0" CACHE STRING "" FORCE)
    set(CMAKE_EXECUTABLE_SUFFIX ".html")
endif()

# ---------------------------------------------------------------------------
# raylib: owns the window, the GL context and all 3D rendering.
# ---------------------------------------------------------------------------
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES    OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG 5.5
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(raylib)

# ---------------------------------------------------------------------------
# fmt
# ---------------------------------------------------------------------------
find_package(fmt CONFIG QUIET)
if(NOT fmt_FOUND)
    FetchContent_Declare(
        fmt
        GIT_REPOSITORY https://github.com/fmtlib/fmt.git
        GIT_TAG 12.2.0
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(fmt)
endif()

# ---------------------------------------------------------------------------
# Dear ImGui (docking) + ImPlot + ImPlot3D, taken from the imgui_bundle forks
# at the exact commits pinned by imgui_bundle v1.92.900 so the C++ API matches
# the Python imgui_bundle used by quick-mag one-to-one.
#
# We deliberately do not use HelloImGui's runner: raylib owns the window, and
# rlImGui (raylib-extras) is the ImGui platform/renderer backend.
# ---------------------------------------------------------------------------
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/pthom/imgui.git
    GIT_TAG c9a0a84f8a4ddae794a9e1acf6a93afce73d5057   # imgui_bundle v1.92.900 (docking, 1.92.9)
    GIT_SHALLOW TRUE
)
FetchContent_Declare(
    implot
    GIT_REPOSITORY https://github.com/pthom/implot.git
    GIT_TAG fa5b4e2f843db64a9a9ccc4357d3a74e4ca0b389   # imgui_bundle v1.92.900
    GIT_SHALLOW TRUE
)
FetchContent_Declare(
    implot3d
    GIT_REPOSITORY https://github.com/pthom/implot3d.git
    GIT_TAG be541bc3fe2397b53a69a380ce7a0e9024de6326   # imgui_bundle v1.92.900
    GIT_SHALLOW TRUE
)
FetchContent_Declare(
    portable_file_dialogs
    GIT_REPOSITORY https://github.com/samhocevar/portable-file-dialogs.git
    GIT_TAG 0.1.0
    GIT_SHALLOW TRUE
    SOURCE_SUBDIR _header_only_   # header-only: skip its (ancient) CMakeLists.txt
)
FetchContent_Declare(
    tomlplusplus
    GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
    GIT_TAG v3.4.0
    GIT_SHALLOW TRUE
    SOURCE_SUBDIR _header_only_   # header-only; we only need its include dir
)
# ---------------------------------------------------------------------------
# imgui-node-editor (node graph canvas), pthom fork pinned at the commit used
# by imgui_bundle v1.92.900 (branch imgui_bundle), matching the imgui pin above.
# nlohmann/json (release include.zip, header-only): JSON layer for the node
# graph's external-script protocol (crude_json stays internal to the editor).
# ---------------------------------------------------------------------------
FetchContent_Declare(
    imgui_node_editor
    GIT_REPOSITORY https://github.com/pthom/imgui-node-editor.git
    GIT_TAG 1d3736d8ca68fd63cfbc3a19d1ad9bed74fce62c   # imgui_bundle v1.92.900
    GIT_SHALLOW TRUE
)
FetchContent_Declare(
    nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.12.0/include.zip
)
FetchContent_Declare(
    rlimgui
    GIT_REPOSITORY https://github.com/raylib-extras/rlImGui.git
    GIT_TAG 1550009359ad975927f7f0e4a3f47e3f27123ea9   # 2026-08-24, ImGui 1.92 texture API
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(imgui implot implot3d portable_file_dialogs tomlplusplus rlimgui imgui_node_editor nlohmann_json)

add_library(chemlab_imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_stacklayout.cpp   # pthom/imgui addition (stack layout)
    ${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp
    ${implot_SOURCE_DIR}/implot.cpp
    ${implot_SOURCE_DIR}/implot_items.cpp
    ${implot_SOURCE_DIR}/implot_demo.cpp
    ${implot3d_SOURCE_DIR}/implot3d.cpp
    ${implot3d_SOURCE_DIR}/implot3d_items.cpp
    ${implot3d_SOURCE_DIR}/implot3d_meshes.cpp
    ${rlimgui_SOURCE_DIR}/rlImGui.cpp
    ${imgui_node_editor_SOURCE_DIR}/imgui_node_editor.cpp
    ${imgui_node_editor_SOURCE_DIR}/imgui_node_editor_api.cpp
    ${imgui_node_editor_SOURCE_DIR}/imgui_canvas.cpp
    ${imgui_node_editor_SOURCE_DIR}/crude_json.cpp
)
target_include_directories(chemlab_imgui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/misc/cpp
    ${implot_SOURCE_DIR}
    ${implot3d_SOURCE_DIR}
    ${rlimgui_SOURCE_DIR}
    ${portable_file_dialogs_SOURCE_DIR}
    ${imgui_node_editor_SOURCE_DIR}
)
target_compile_definitions(chemlab_imgui PUBLIC
    IMGUI_DEFINE_MATH_OPERATORS
    NO_FONT_AWESOME
)
target_link_libraries(chemlab_imgui PUBLIC raylib)

# ---------------------------------------------------------------------------
# Embedded UI font: Roboto-Medium from ImGui's misc/fonts, compressed into a
# C array at build time so the binary does not depend on the assets folder.
# ---------------------------------------------------------------------------
# The tool has to run on the build machine. Cross-compiling (Emscripten) would
# otherwise turn it into a wasm module CMake cannot execute, so build it with
# the host compiler at configure time instead.
set(CHEMLAB_FONT_SRC ${CMAKE_CURRENT_BINARY_DIR}/generated/font_roboto_medium.c)
if(CMAKE_CROSSCOMPILING)
    find_program(CHEMLAB_HOST_CXX NAMES c++ g++ clang++ REQUIRED)
    set(CHEMLAB_FONT_TOOL ${CMAKE_CURRENT_BINARY_DIR}/binary_to_compressed_c_host)
    execute_process(
        COMMAND ${CHEMLAB_HOST_CXX} -O2 -o ${CHEMLAB_FONT_TOOL}
                ${imgui_SOURCE_DIR}/misc/fonts/binary_to_compressed_c.cpp
        RESULT_VARIABLE CHEMLAB_FONT_TOOL_RESULT
    )
    if(NOT CHEMLAB_FONT_TOOL_RESULT EQUAL 0)
        message(FATAL_ERROR "Failed to build binary_to_compressed_c with the host compiler")
    endif()
    set(CHEMLAB_FONT_TOOL_DEP ${CHEMLAB_FONT_TOOL})
else()
    add_executable(binary_to_compressed_c ${imgui_SOURCE_DIR}/misc/fonts/binary_to_compressed_c.cpp)
    set(CHEMLAB_FONT_TOOL binary_to_compressed_c)
    set(CHEMLAB_FONT_TOOL_DEP binary_to_compressed_c)
endif()
add_custom_command(
    OUTPUT ${CHEMLAB_FONT_SRC}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/generated
    COMMAND ${CHEMLAB_FONT_TOOL} -nostatic ${imgui_SOURCE_DIR}/misc/fonts/Roboto-Medium.ttf roboto_medium > ${CHEMLAB_FONT_SRC}
    DEPENDS ${CHEMLAB_FONT_TOOL_DEP} ${imgui_SOURCE_DIR}/misc/fonts/Roboto-Medium.ttf
    COMMENT "Embedding Roboto-Medium.ttf"
    VERBATIM
)

# ---------------------------------------------------------------------------
# ChemLab
# ---------------------------------------------------------------------------
add_executable(${PROJECT_NAME}
    ${CHEMLAB_FONT_SRC}
    src/main.cpp
    src/core/molecule.cpp
    src/core/chemical_data.cpp
    src/core/element.cpp
    src/core/xyz_io.cpp
    src/render/shaders.cpp
    src/render/impostor_renderer.cpp
    src/render/molecular_model.cpp
    src/render/orbit_camera.cpp
    src/render/viewport.cpp
    src/app/app_state.cpp
    src/app/actions.cpp
    src/app/commands.cpp
    src/app/project.cpp
    src/plot/plot_spec.cpp
    src/graph/value.cpp
    src/graph/graph.cpp
    src/graph/node_registry.cpp
    src/graph/nodes_builtin.cpp
    src/graph/nodes_plot.cpp
    src/graph/nodes_view.cpp
    src/graph/node_python.cpp
    src/graph/py_runner.cpp
    src/graph/graph_commands.cpp
    src/graph/graph_io.cpp
    src/graph/scene.cpp
    src/ui/theme.cpp
    src/ui/ui.cpp
    src/ui/ui_spec.cpp
    src/ui/ui_builder.cpp
    src/ui/panel_registry.cpp
    src/ui/node_views.cpp
    src/ui/command_bar.cpp
    src/ui/panel_controls.cpp
    src/ui/panel_structure_view.cpp
    src/ui/panel_plot.cpp
    src/ui/panel_active_structure.cpp
    src/ui/panel_calculate.cpp
    src/ui/panel_output.cpp
    src/ui/panel_export.cpp
    src/ui/panel_node_graph.cpp
)

target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${tomlplusplus_SOURCE_DIR}/include
        ${nlohmann_json_SOURCE_DIR}/include
        ${raylib_SOURCE_DIR}/src
        ${raylib_SOURCE_DIR}/examples/shaders   # rlights.h
)

target_link_libraries(${PROJECT_NAME} PRIVATE chemlab_imgui fmt::fmt raylib)
if(NOT EMSCRIPTEN)
    target_link_libraries(${PROJECT_NAME} PRIVATE Threads::Threads)
endif()
target_compile_definitions(${PROJECT_NAME} PRIVATE RAYMATH_DISABLE_CPP_OPERATORS)

if(EMSCRIPTEN)
    # assets/ is baked into the .data package and mounted at /assets.
    target_compile_definitions(${PROJECT_NAME} PRIVATE ASSETS_PATH="/assets/")
    target_compile_options(${PROJECT_NAME} PRIVATE -fexceptions)
    target_compile_options(chemlab_imgui PRIVATE -fexceptions)
    target_link_options(${PROJECT_NAME} PRIVATE
        -fexceptions                       # xyz/toml parsing reports errors by throwing
        -sALLOW_MEMORY_GROWTH=1            # big trajectories are loaded at runtime
        -sINITIAL_MEMORY=268435456         # 256 MB, enough for the bundled samples
        -sSTACK_SIZE=4194304                # 4 MB; the default 64 kB is not enough for ImGui
        -sFORCE_FILESYSTEM=1               # dropped files land in MEMFS
        --preload-file ${CMAKE_CURRENT_SOURCE_DIR}/assets@/assets
        --shell-file ${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html
    )
    set_target_properties(${PROJECT_NAME} PROPERTIES
        LINK_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html)
else()
    target_compile_definitions(${PROJECT_NAME} PRIVATE ASSETS_PATH="${CMAKE_CURRENT_SOURCE_DIR}/assets/")
endif()

if(APPLE)
    target_link_libraries(${PROJECT_NAME} PRIVATE "-framework IOKit" "-framework Cocoa" "-framework OpenGL")
endif()
