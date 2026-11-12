cmake_minimum_required(VERSION 3.25)

# C is only needed for the generated glad loader.
project(Manifold VERSION 0.0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

option(MANIFOLD_BUILD_TESTS "Build the unit tests" ON)

include(FetchContent)
# SYSTEM hides warnings coming from third-party headers.
FetchContent_Declare(glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.1
    GIT_SHALLOW TRUE
    SYSTEM
)
FetchContent_MakeAvailable(glm)

set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
FetchContent_Declare(SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-3.4.16
    GIT_SHALLOW TRUE
    SYSTEM
)
FetchContent_MakeAvailable(SDL3)

# stb has no CMake support, so it is only downloaded and exposed as an include directory.
FetchContent_Declare(stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG 003501f6d7a8668ce8b6d875e34dd97ad0d5d5b5
)
FetchContent_MakeAvailable(stb)
add_library(stb INTERFACE)
target_include_directories(stb SYSTEM INTERFACE ${stb_SOURCE_DIR})

# glad loads OpenGL functions. It is generated code kept in the repo, see external/glad/README.md.
add_library(glad STATIC external/glad/src/gl.c)
target_include_directories(glad SYSTEM PUBLIC external/glad/include)

set(MANIFOLD_WARNINGS $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>)

# Game logic and everything else that doesn't need a window or OpenGL lives in a library,
# so that tests can link against it.
add_library(manifold_core STATIC
    src/game.cpp
    src/level/debug_layout.cpp
    src/level/layout_factory.cpp
    src/level/level.cpp
    src/level/room_layout.cpp
    src/physics/collision.cpp
    src/render/camera.cpp
    src/render/lighting.cpp
    src/render/shadow_map.cpp
    src/render/stb_image.cpp
    src/render/stb_image_write.cpp
    src/render/texture_image.cpp
    src/render/viewport.cpp
    src/render/world_mesh.cpp
    src/util/log.cpp
    src/world/geometry.cpp
    src/world/player.cpp
    src/world/torch.cpp
    src/world/world.cpp
)
target_include_directories(manifold_core PUBLIC src)
target_link_libraries(manifold_core PUBLIC glm::glm stb)
target_compile_options(manifold_core PRIVATE ${MANIFOLD_WARNINGS})

add_executable(manifold
    src/main.cpp
    src/platform/window.cpp
    src/render/renderer.cpp
    src/render/screenshot.cpp
)
target_link_libraries(manifold PRIVATE manifold_core SDL3::SDL3 glad)
target_compile_options(manifold PRIVATE ${MANIFOLD_WARNINGS})
# Textures are read from the source tree, so the game finds them however it is started.
target_compile_definitions(manifold PRIVATE MANIFOLD_TEXTURES_DIR="${CMAKE_SOURCE_DIR}/assets/textures")

if(MANIFOLD_BUILD_TESTS)
    enable_testing()
    FetchContent_Declare(googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.17.0
        GIT_SHALLOW TRUE
        SYSTEM
    )
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    add_executable(manifold_tests
        tests/camera_test.cpp
        tests/collision_test.cpp
        tests/game_test.cpp
        tests/geometry_test.cpp
        tests/debug_layout_test.cpp
        tests/layout_factory_test.cpp
        tests/lighting_test.cpp
        tests/log_test.cpp
        tests/room_layout_test.cpp
        tests/shadow_map_test.cpp
        tests/texture_image_test.cpp
        tests/torch_test.cpp
        tests/viewport_test.cpp
        tests/world_mesh_test.cpp
        tests/world_test.cpp
    )
    target_link_libraries(manifold_tests PRIVATE manifold_core GTest::gtest_main)
    target_compile_options(manifold_tests PRIVATE ${MANIFOLD_WARNINGS})

    include(GoogleTest)
    gtest_discover_tests(manifold_tests)
endif()

# Code style: `format` fixes formatting in place, `lint` checks formatting and runs clang-tidy.
file(GLOB_RECURSE MANIFOLD_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_SOURCE_DIR}/src/*.cpp
    ${CMAKE_SOURCE_DIR}/src/*.hpp
    ${CMAKE_SOURCE_DIR}/tests/*.cpp
    ${CMAKE_SOURCE_DIR}/tests/*.hpp
)
file(GLOB_RECURSE MANIFOLD_TU_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/src/*.cpp ${CMAKE_SOURCE_DIR}/tests/*.cpp)

find_program(CLANG_FORMAT_EXE clang-format)
find_program(CLANG_TIDY_EXE clang-tidy)

if(CLANG_FORMAT_EXE)
    add_custom_target(format
        COMMAND ${CLANG_FORMAT_EXE} -i ${MANIFOLD_SOURCES}
        COMMENT "Formatting sources with clang-format"
        VERBATIM
    )
    add_custom_target(format-check
        COMMAND ${CLANG_FORMAT_EXE} --dry-run --Werror ${MANIFOLD_SOURCES}
        COMMENT "Checking formatting with clang-format"
        VERBATIM
    )
else()
    foreach(target format format-check)
        add_custom_target(${target}
            COMMAND ${CMAKE_COMMAND} -E echo "clang-format not found: sudo apt install clang-format, then re-run cmake -S . -B build"
            COMMAND ${CMAKE_COMMAND} -E false
            VERBATIM
        )
    endforeach()
endif()

if(CLANG_TIDY_EXE)
    add_custom_target(lint
        COMMAND ${CLANG_TIDY_EXE} -p ${CMAKE_BINARY_DIR} ${MANIFOLD_TU_SOURCES}
        COMMENT "Running clang-tidy"
        VERBATIM
    )
else()
    add_custom_target(lint
        COMMAND ${CMAKE_COMMAND} -E echo "clang-tidy not found: sudo apt install clang-tidy, then re-run cmake -S . -B build"
        COMMAND ${CMAKE_COMMAND} -E false
        VERBATIM
    )
endif()
add_dependencies(lint format-check)
