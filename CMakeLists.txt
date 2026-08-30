cmake_minimum_required(VERSION 3.18)
project(ChemLab LANGUAGES C CXX)

include(FetchContent)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

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
        GIT_TAG 11.2.0
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
FetchContent_Declare(
    rlimgui
    GIT_REPOSITORY https://github.com/raylib-extras/rlImGui.git
    GIT_TAG 1550009359ad975927f7f0e4a3f47e3f27123ea9   # 2026-08-24, ImGui 1.92 texture API
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(imgui implot implot3d portable_file_dialogs tomlplusplus rlimgui)

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
)
target_include_directories(chemlab_imgui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/misc/cpp
    ${implot_SOURCE_DIR}
    ${implot3d_SOURCE_DIR}
    ${rlimgui_SOURCE_DIR}
    ${portable_file_dialogs_SOURCE_DIR}
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
add_executable(binary_to_compressed_c ${imgui_SOURCE_DIR}/misc/fonts/binary_to_compressed_c.cpp)
set(CHEMLAB_FONT_SRC ${CMAKE_CURRENT_BINARY_DIR}/generated/font_roboto_medium.c)
add_custom_command(
    OUTPUT ${CHEMLAB_FONT_SRC}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/generated
    COMMAND binary_to_compressed_c -nostatic ${imgui_SOURCE_DIR}/misc/fonts/Roboto-Medium.ttf roboto_medium > ${CHEMLAB_FONT_SRC}
    DEPENDS binary_to_compressed_c ${imgui_SOURCE_DIR}/misc/fonts/Roboto-Medium.ttf
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
    src/core/xyz_io.cpp
    src/render/shaders.cpp
    src/render/molecular_model.cpp
    src/render/orbit_camera.cpp
    src/render/viewport.cpp
    src/app/app_state.cpp
    src/app/actions.cpp
    src/app/commands.cpp
    src/app/project.cpp
    src/ui/theme.cpp
    src/ui/ui.cpp
    src/ui/ui_spec.cpp
    src/ui/ui_builder.cpp
    src/ui/panel_registry.cpp
    src/ui/command_bar.cpp
    src/ui/panel_controls.cpp
    src/ui/panel_structure_view.cpp
    src/ui/panel_active_structure.cpp
    src/ui/panel_calculate.cpp
    src/ui/panel_output.cpp
    src/ui/panel_export.cpp
)

target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${tomlplusplus_SOURCE_DIR}/include
        ${raylib_SOURCE_DIR}/src
        ${raylib_SOURCE_DIR}/examples/shaders   # rlights.h
)

target_link_libraries(${PROJECT_NAME} PRIVATE chemlab_imgui fmt::fmt Threads::Threads raylib)
target_compile_definitions(${PROJECT_NAME}
    PRIVATE
        ASSETS_PATH="${CMAKE_CURRENT_SOURCE_DIR}/assets/"
        RAYMATH_DISABLE_CPP_OPERATORS
)

if(APPLE)
    target_link_libraries(${PROJECT_NAME} PRIVATE "-framework IOKit" "-framework Cocoa" "-framework OpenGL")
endif()
