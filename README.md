# Manifold

A hobby first-person fantasy game about playing as a mage. See [docs/ROADMAP.md](docs/ROADMAP.md).

## Building (Linux)

Requirements: a C++20 compiler (GCC 13+ or Clang 16+), CMake 3.25+, and optionally Ninja.
On Ubuntu 24.04, everything needed to build and lint the project is installed with:

```sh
sudo apt install build-essential cmake ninja-build git clang-format clang-tidy pkg-config libepoxy-dev \
    libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxfixes-dev libxi-dev libxss-dev libxtst-dev \
    libxkbcommon-dev libwayland-dev wayland-protocols libdecor-0-dev libegl1-mesa-dev libgl1-mesa-dev \
    libasound2-dev libpulse-dev libpipewire-0.3-dev libdbus-1-dev libudev-dev
```

The first line is the toolchain and libepoxy, which loads OpenGL functions. The rest are system development
packages that SDL3 needs to talk to X11/Wayland, the GPU, audio, the desktop (D-Bus) and input devices (udev).
The full list of SDL's optional dependencies is in its
[README-linux](https://wiki.libsdl.org/SDL3/README-linux#build-dependencies). SDL3 itself and the other libraries
(GLM, stb, GoogleTest) are downloaded and built by CMake.

Build and run:

```sh
cmake -S . -B build -G Ninja
cmake --build build
./build/manifold                            # random level
./build/manifold 42                         # level generated from seed 42
./build/manifold 42 --layout rooms          # force a layout: debug or rooms (by default rooms; debug only on request)
./build/manifold 42 --screenshot view.png   # save the first frame as a PNG and exit
```

Controls are described in [GAME_MANUAL.md](GAME_MANUAL.md). Press Escape or close the window to quit.

## Tests

Unit tests use GoogleTest, which CMake downloads. Build as above, then:

```sh
ctest --test-dir build --output-on-failure
```

Pass `-DMANIFOLD_BUILD_TESTS=OFF` to the first `cmake` command to skip building the tests.

## Code style

Formatting is defined in `.clang-format` (Google style, 4-space indent, 120 columns) and
lint rules, including naming conventions, in `.clang-tidy`.

```sh
cmake --build build --target format   # reformat all sources in place
cmake --build build --target lint     # check formatting, then run clang-tidy (fails on any issue)
```

See [docs/CODING_STYLE.md](docs/CODING_STYLE.md) for all conventions, including doc comments.

Naming: types `PascalCase`; functions, variables and namespaces `snake_case`; private and
protected members `snake_case_`; constants and enum values `kPascalCase`; macros `UPPER_CASE`.
