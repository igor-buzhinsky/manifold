# Fails if the game's sources make random choices without the helpers of src/util/random.hpp.
#
# The standard distributions, std::shuffle and std::sample give different results with different standard libraries,
# so a seed would give different levels on different platforms. See CODING_STYLE.md. Run as a test:
#   cmake -DSOURCE_DIR=<repository> -P cmake/check_random.cmake

if(NOT SOURCE_DIR)
    message(FATAL_ERROR "SOURCE_DIR is not set")
endif()

# What may only be used in src/util/random.*: distributions, algorithms that draw random numbers themselves, also
# unqualified (argument-dependent lookup finds std::shuffle for std iterators), other engines than Rng, and the C
# library's generator. The check is textual, so a comment that names them fails it too: refer to them in comments
# without `std::` and without the parenthesis or angle bracket after their name.
set(BANNED
    "_distribution[ \t]*<"
    "_distribution[ \t]*\\("
    "std::(ranges::)?(shuffle|sample|random_shuffle)"
    "(^|[^_a-zA-Z0-9.>:])(shuffle|sample|s?rand)[ \t]*\\("
    "generate_canonical"
    "std::(mt19937|minstd_rand|ranlux|knuth_b|default_random_engine|random_device)"
    "_engine[ \t]*<"
    "std::s?rand[ \t]*\\("
)
# random_device seeds a new game, which is meant to differ, so main.cpp may use it.
set(ALLOWED_RANDOM_DEVICE "${SOURCE_DIR}/src/main.cpp")

file(GLOB_RECURSE files "${SOURCE_DIR}/src/*.cpp" "${SOURCE_DIR}/src/*.hpp" "${SOURCE_DIR}/tests/*.cpp"
     "${SOURCE_DIR}/tests/*.hpp")
set(failures "")
foreach(file IN LISTS files)
    if(file MATCHES "/src/util/random\\.(cpp|hpp)$")
        continue()
    endif()
    file(READ "${file}" text)
    # Semicolons would split the matched lines, which CMake keeps in lists.
    string(REPLACE ";" "," text "${text}")
    foreach(pattern IN LISTS BANNED)
        string(REGEX MATCHALL "[^\n]*${pattern}[^\n]*" found "${text}")
        foreach(line IN LISTS found)
            if(file STREQUAL ALLOWED_RANDOM_DEVICE AND line MATCHES "std::random_device")
                continue()
            endif()
            string(STRIP "${line}" line)
            string(APPEND failures "\n  ${file}: ${line}")
        endforeach()
    endforeach()
endforeach()
if(failures)
    message(FATAL_ERROR "Use the helpers of src/util/random.hpp instead of the standard library's random choices, "
                        "which differ between platforms:${failures}")
endif()
