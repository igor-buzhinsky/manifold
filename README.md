# Manifold

A hobby first-person fantasy game about playing as a mage. See [docs/roadmap.md](docs/roadmap.md).

## Building (Linux)

Requirements: a C++20 compiler (GCC 13+ or Clang 16+), CMake 3.24+, and optionally Ninja.
On Ubuntu 24.04, everything needed to build and lint the project is installed with:

```sh
sudo apt install build-essential cmake ninja-build git clang-format clang-tidy
```

Third-party libraries (SDL3, Jolt and others) will be downloaded and built by CMake, so they don't need to be
installed. SDL3 still needs these system development packages to talk to X11/Wayland, the GPU and audio.
They aren't needed until SDL3 is added, but you can install them in advance:

```sh
sudo apt install libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxi-dev libxss-dev \
    libxkbcommon-dev libwayland-dev wayland-protocols libegl1-mesa-dev libgl1-mesa-dev \
    libasound2-dev libpulse-dev libdecor-0-dev
```

Build and run:

```sh
cmake -S . -B build -G Ninja
cmake --build build
./build/manifold         # random level
./build/manifold 42      # level generated from seed 42
```

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

Naming: types `PascalCase`; functions, variables and namespaces `snake_case`; private and
protected members `snake_case_`; constants and enum values `kPascalCase`; macros `UPPER_CASE`.
