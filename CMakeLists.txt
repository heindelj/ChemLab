cmake_minimum_required(VERSION 3.16)
project(ChemLab LANGUAGES CXX)

include(FetchContent)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# Build raylib from source so macOS framework/linker settings match the host.
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE) # don't build the supplied examples
set(BUILD_GAMES    OFF CACHE BOOL "" FORCE) # or games
FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG 5.5
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(raylib)

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

add_executable(${PROJECT_NAME} src/main.cpp)

target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${raylib_SOURCE_DIR}/src
        ${raylib_SOURCE_DIR}/examples/models
        ${CMAKE_CURRENT_SOURCE_DIR}/external/raylib/include
)

target_link_libraries(${PROJECT_NAME} PRIVATE fmt::fmt Threads::Threads raylib)
target_compile_definitions(${PROJECT_NAME}
    PRIVATE
        ASSETS_PATH="${CMAKE_CURRENT_SOURCE_DIR}/assets/" # Set the asset path macro to the absolute path on the dev machine
        RAYMATH_DISABLE_CPP_OPERATORS
)
#target_compile_definitions(${PROJECT_NAME} PUBLIC ASSETS_PATH="relative-path-to-assets-in-the-game-package") # Set the asset path macro in release more
