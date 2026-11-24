# The libraries that CMake downloads, with their versions. CMakeLists.txt includes this file and makes them available.
#
# CI caches the downloaded sources and keys the cache on this file's hash, so keep here only what decides what is
# downloaded: any change to this file makes CI download every library again, and one to how a library is built
# belongs in CMakeLists.txt.

include(FetchContent)

# SYSTEM hides warnings coming from third-party headers.
FetchContent_Declare(glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.1
    GIT_SHALLOW TRUE
    SYSTEM
)

FetchContent_Declare(SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-3.4.16
    GIT_SHALLOW TRUE
    SYSTEM
)

# stb has no CMake support, so it is only downloaded and exposed as an include directory.
FetchContent_Declare(stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG 003501f6d7a8668ce8b6d875e34dd97ad0d5d5b5
)

# nlohmann/json reads and writes save files. The release archive has only the headers and the CMake files, which
# is much smaller than the repository.
FetchContent_Declare(nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
    URL_HASH SHA256=42f6e95cad6ec532fd372391373363b62a14af6d771056dbfc86160e6dfff7aa
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SYSTEM
)

# Dear ImGui draws the debug overlay. It has no CMake support, so its sources are built in CMakeLists.txt.
FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.93.0
    GIT_SHALLOW TRUE
)

# Only downloaded when the tests are built.
FetchContent_Declare(googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.17.0
    GIT_SHALLOW TRUE
    SYSTEM
)
