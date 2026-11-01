cmake_minimum_required(VERSION 3.24)

project(Manifold VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

add_executable(manifold
    src/main.cpp
)

target_compile_options(manifold PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
)

# Code style: `format` fixes formatting in place, `lint` checks formatting and runs clang-tidy.
file(GLOB_RECURSE MANIFOLD_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_SOURCE_DIR}/src/*.cpp
    ${CMAKE_SOURCE_DIR}/src/*.hpp
)
file(GLOB_RECURSE MANIFOLD_TU_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/src/*.cpp)

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
