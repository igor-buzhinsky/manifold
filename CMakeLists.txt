cmake_minimum_required(VERSION 3.25)

# C is only needed for the generated glad loader.
project(Manifold VERSION 0.3.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

option(MANIFOLD_BUILD_TESTS "Build the unit tests" ON)

include(cmake/dependencies.cmake)
FetchContent_MakeAvailable(glm)

set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(SDL3)

# stb has no CMake support, so it is only exposed as an include directory.
FetchContent_MakeAvailable(stb)
add_library(stb INTERFACE)
target_include_directories(stb SYSTEM INTERFACE ${stb_SOURCE_DIR})
# stb_vorbis decodes OGG Vorbis sounds. It is one C file, built here without our warnings; the pushdata API (for
# decoding streams piece by piece) isn't needed.
add_library(stb_vorbis STATIC ${stb_SOURCE_DIR}/stb_vorbis.c)
target_compile_definitions(stb_vorbis PUBLIC STB_VORBIS_NO_PUSHDATA_API)
target_link_libraries(stb_vorbis PUBLIC stb)

FetchContent_MakeAvailable(nlohmann_json)

# Dear ImGui has no CMake support, so its sources, with the backends for SDL3 and OpenGL 3+, are built here.
FetchContent_MakeAvailable(imgui)
add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
)
target_include_directories(imgui SYSTEM PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
# The OpenGL backend loads OpenGL functions itself with dlopen, which older glibc has in libdl.
target_link_libraries(imgui PUBLIC SDL3::SDL3 PRIVATE ${CMAKE_DL_LIBS})
# Keeps the game from using names that ImGui is about to remove.
target_compile_definitions(imgui PUBLIC IMGUI_DISABLE_OBSOLETE_FUNCTIONS)

# glad loads OpenGL functions. It is generated code kept in the repo, see external/glad/README.md.
add_library(glad STATIC external/glad/src/gl.c)
target_include_directories(glad SYSTEM PUBLIC external/glad/include)

set(MANIFOLD_WARNINGS $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>)
# CI turns this on. Unlike CMAKE_COMPILE_WARNING_AS_ERROR, it leaves the third-party libraries alone.
option(MANIFOLD_WARNINGS_AS_ERRORS "Treat compiler warnings in Manifold's own code as errors" OFF)
if(MANIFOLD_WARNINGS_AS_ERRORS)
    list(APPEND MANIFOLD_WARNINGS $<$<CXX_COMPILER_ID:GNU,Clang>:-Werror>)
endif()

# Game logic and everything else that doesn't need a window or OpenGL lives in a library,
# so that tests can link against it.
add_library(manifold_core STATIC
    src/audio/sound.cpp
    src/audio/sound_set.cpp
    src/audio/spell_sounds.cpp
    src/console/console.cpp
    src/console/console_window.cpp
    src/console/game_commands.cpp
    src/data/game_data.cpp
    src/game.cpp
    src/items/inventory.cpp
    src/items/item_catalog.cpp
    src/items/loot.cpp
    src/level/debug_layout.cpp
    src/level/layout_factory.cpp
    src/level/level.cpp
    src/level/room_layout.cpp
    src/level/texture_kind.cpp
    src/party/party.cpp
    src/party/spells.cpp
    src/physics/collision.cpp
    src/physics/raycast.cpp
    src/render/camera.cpp
    src/render/lighting.cpp
    src/render/marker.cpp
    src/render/shadow_map.cpp
    src/render/stb_image.cpp
    src/render/stb_image_write.cpp
    src/render/texture_image.cpp
    src/render/viewport.cpp
    src/render/world_mesh.cpp
    src/save/saved_game.cpp
    src/settings/settings.cpp
    src/sim/movement_sim.cpp
    src/ui/button_list.cpp
    src/ui/crosshair.cpp
    src/ui/game_menus.cpp
    src/ui/inventory_screen.cpp
    src/ui/item_grid.cpp
    src/ui/menu_screen.cpp
    src/ui/mouse_look.cpp
    src/ui/overlay_mesh.cpp
    src/ui/party_hud.cpp
    src/ui/screen_stack.cpp
    src/ui/spell_hud.cpp
    src/ui/text_field.cpp
    src/ui/ui_style.cpp
    src/util/asset_files.cpp
    src/util/asset_mapping.cpp
    src/util/frame_times.cpp
    src/util/log.cpp
    src/util/random.cpp
    src/util/text_file.cpp
    src/world/chest.cpp
    src/world/door.cpp
    src/world/geometry.cpp
    src/world/obstacle.cpp
    src/world/player.cpp
    src/world/spatial_grid.cpp
    src/world/torch.cpp
    src/world/world.cpp
)
target_include_directories(manifold_core PUBLIC src)
target_link_libraries(manifold_core PUBLIC glm::glm stb stb_vorbis PRIVATE nlohmann_json::nlohmann_json)
target_compile_options(manifold_core PRIVATE ${MANIFOLD_WARNINGS})

add_executable(manifold
    src/main.cpp
    src/platform/audio.cpp
    src/platform/terminal_input.cpp
    src/platform/window.cpp
    src/render/gl_util.cpp
    src/render/image_renderer.cpp
    src/render/overlay_renderer.cpp
    src/render/renderer.cpp
    src/render/screenshot.cpp
    src/ui/debug_overlay.cpp
)
target_link_libraries(manifold PRIVATE manifold_core SDL3::SDL3 glad imgui)
target_compile_options(manifold PRIVATE ${MANIFOLD_WARNINGS})
# Plays levels with random input and checks invariants of the player's movement, see src/sim/movement_sim.hpp. It needs
# no window, and isn't part of a copy of the game.
add_executable(manifold_sim src/sim/sim_main.cpp)
target_link_libraries(manifold_sim PRIVATE manifold_core)
target_compile_options(manifold_sim PRIVATE ${MANIFOLD_WARNINGS})
# The simulation plays with the settings of the game's data files in the source tree.
target_compile_definitions(manifold_sim PRIVATE MANIFOLD_DATA_DIR="${CMAKE_SOURCE_DIR}/data")
# Textures and sounds are read from `assets`, and the game's data such as its items from `data`, next to the executable
# if they are there, as in a copy of the game made by `cmake --install`, and otherwise from the source tree, so that
# the game in the build directory finds them too.
target_compile_definitions(manifold PRIVATE MANIFOLD_ASSETS_DIR="${CMAKE_SOURCE_DIR}/assets"
                                            MANIFOLD_DATA_DIR="${CMAKE_SOURCE_DIR}/data")
# The game's own settings are read the same way: `settings.json` next to the executable, or else the one in the source
# tree. The player's settings in the directory of the saves take priority over them.
target_compile_definitions(manifold PRIVATE MANIFOLD_SETTINGS_FILE="${CMAKE_SOURCE_DIR}/settings.json")
# `cmake --install build --prefix DIR` makes a copy of the game that runs from anywhere: DIR/manifold and DIR/assets,
# with the game manual, DEBUGGING.md, the game's settings and the licenses of the game and of the libraries built into
# it. Releases are such copies.
# The layout isn't the usual one of /usr/local, so without --prefix the copy goes into the build directory.
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/install" CACHE PATH "Where `cmake --install` puts the game" FORCE)
endif()
install(TARGETS manifold RUNTIME DESTINATION .)
install(DIRECTORY assets/ DESTINATION assets)
install(DIRECTORY data/ DESTINATION data)
install(FILES LICENSE GAME_MANUAL.md DEBUGGING.md settings.json DESTINATION .)
install(FILES ${sdl3_SOURCE_DIR}/LICENSE.txt DESTINATION licenses RENAME SDL3.txt)
install(FILES ${glm_SOURCE_DIR}/copying.txt DESTINATION licenses RENAME glm.txt)
install(FILES ${stb_SOURCE_DIR}/LICENSE DESTINATION licenses RENAME stb.txt)
install(FILES ${imgui_SOURCE_DIR}/LICENSE.txt DESTINATION licenses RENAME imgui.txt)
install(FILES ${nlohmann_json_SOURCE_DIR}/LICENSE.MIT DESTINATION licenses RENAME nlohmann_json.txt)
install(FILES external/glad/LICENSE DESTINATION licenses RENAME glad.txt)

if(MANIFOLD_BUILD_TESTS)
    enable_testing()
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    add_executable(manifold_tests
        tests/asset_files_test.cpp
        tests/asset_mapping_test.cpp
        tests/button_list_test.cpp
        tests/camera_test.cpp
        tests/chest_test.cpp
        tests/collision_test.cpp
        tests/console_test.cpp
        tests/console_window_test.cpp
        tests/crosshair_test.cpp
        tests/door_test.cpp
        tests/game_commands_test.cpp
        tests/game_test.cpp
        tests/inventory_screen_test.cpp
        tests/inventory_test.cpp
        tests/item_catalog_test.cpp
        tests/geometry_test.cpp
        tests/debug_layout_test.cpp
        tests/frame_times_test.cpp
        tests/game_data_test.cpp
        tests/layout_factory_test.cpp
        tests/level_test.cpp
        tests/lighting_test.cpp
        tests/log_test.cpp
        tests/loot_test.cpp
        tests/marker_test.cpp
        tests/menu_screen_test.cpp
        tests/mouse_look_test.cpp
        tests/movement_sim_test.cpp
        tests/overlay_mesh_test.cpp
        tests/party_hud_test.cpp
        tests/spell_hud_test.cpp
        tests/party_test.cpp
        tests/spells_test.cpp
        tests/spell_sounds_test.cpp
        tests/random_test.cpp
        tests/raycast_test.cpp
        tests/text_field_test.cpp
        tests/room_layout_test.cpp
        tests/saved_game_test.cpp
        tests/screen_stack_test.cpp
        tests/settings_test.cpp
        tests/shadow_map_test.cpp
        tests/sound_set_test.cpp
        tests/sound_test.cpp
        tests/spatial_grid_test.cpp
        tests/text_file_test.cpp
        tests/texture_image_test.cpp
        tests/torch_test.cpp
        tests/viewport_test.cpp
        tests/world_mesh_test.cpp
        tests/world_test.cpp
    )
    target_link_libraries(manifold_tests PRIVATE manifold_core GTest::gtest_main nlohmann_json::nlohmann_json)
    target_compile_options(manifold_tests PRIVATE ${MANIFOLD_WARNINGS})
    # The sound tests decode a sound of the game, since no OGG encoder is at hand to make one, the asset mapping tests
    # check the game's mapping and its files, and the item tests the game's items.
    target_compile_definitions(manifold_tests PRIVATE MANIFOLD_ASSETS_DIR="${CMAKE_SOURCE_DIR}/assets"
                                                      MANIFOLD_DATA_DIR="${CMAKE_SOURCE_DIR}/data")

    include(GoogleTest)
    # A test that runs longer than this fails, so that one that hangs, e.g. in a loop waiting for the player to land,
    # is named at once instead of stopping the whole run.
    set(MANIFOLD_TEST_TIMEOUT 60)
    gtest_discover_tests(manifold_tests PROPERTIES TIMEOUT ${MANIFOLD_TEST_TIMEOUT})
    # Random choices must be the same on every platform, see cmake/check_random.cmake.
    add_test(NAME PortableRandomChoices COMMAND ${CMAKE_COMMAND} -DSOURCE_DIR=${CMAKE_SOURCE_DIR}
             -P ${CMAKE_SOURCE_DIR}/cmake/check_random.cmake)
    set_tests_properties(PortableRandomChoices PROPERTIES TIMEOUT ${MANIFOLD_TEST_TIMEOUT})
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
# Runs clang-tidy on many files at once; it comes with clang-tidy.
find_program(RUN_CLANG_TIDY_EXE NAMES run-clang-tidy run-clang-tidy.py)

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

if(CLANG_TIDY_EXE AND RUN_CLANG_TIDY_EXE)
    # The file arguments are escaped, anchored regexes on paths in compile_commands.json, which keeps out
    # dependencies. Only files in the database are linted: tests only when MANIFOLD_BUILD_TESTS is on.
    set(LINT_FILE_REGEXES)
    foreach(source ${MANIFOLD_TU_SOURCES})
        string(REGEX REPLACE "([][+.*()^$?|\\{}])" "\\\\\\1" escaped_source "${source}")
        list(APPEND LINT_FILE_REGEXES "^${escaped_source}$")
    endforeach()
    add_custom_target(lint
        COMMAND ${RUN_CLANG_TIDY_EXE} -quiet -clang-tidy-binary ${CLANG_TIDY_EXE} -p ${CMAKE_BINARY_DIR}
            ${LINT_FILE_REGEXES}
        COMMENT "Running clang-tidy on all cores"
        VERBATIM
    )
else()
    add_custom_target(lint
        COMMAND ${CMAKE_COMMAND} -E echo "clang-tidy or run-clang-tidy not found: sudo apt install clang-tidy, then re-run cmake -S . -B build"
        COMMAND ${CMAKE_COMMAND} -E false
        VERBATIM
    )
endif()
add_dependencies(lint format-check)
